// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


#define NBUCKET 13

struct {
  struct spinlock lock;       // only used when stealing buffers
  struct buf buf[NBUF];

  struct {
    struct spinlock lock;
    struct buf head;
  } bucket[NBUCKET];
} bcache;

static uint
bhash(uint dev, uint blockno)
{
  return (dev + blockno) % NBUCKET;
}


void
binit(void)
{
  struct buf *b;
  int i = 0;

  initlock(&bcache.lock, "bcache");

  for(i = 0; i < NBUCKET; i++){
    initlock(&bcache.bucket[i].lock, "bcache.bucket");
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }

  i = 0;
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    initsleeplock(&b->lock, "buffer");

    b->next = bcache.bucket[i].head.next;
    b->prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next->prev = b;
    bcache.bucket[i].head.next = b;

    i = (i + 1) % NBUCKET;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint h = bhash(dev, blockno);

  acquire(&bcache.bucket[h].lock);

  // Is the block already cached?
  for(b = bcache.bucket[h].head.next; b != &bcache.bucket[h].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[h].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bucket[h].lock);

  // Miss: serialize stealing/recycling to avoid duplicate cached blocks.
  acquire(&bcache.lock);
  acquire(&bcache.bucket[h].lock);

  // Check again after acquiring the miss lock.
  for(b = bcache.bucket[h].head.next; b != &bcache.bucket[h].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[h].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Recycle an unused buffer from this bucket or another bucket.
  for(int n = 0; n < NBUCKET; n++){
    uint i = (h + n) % NBUCKET;

    if(i != h)
      acquire(&bcache.bucket[i].lock);

    for(b = bcache.bucket[i].head.prev; b != &bcache.bucket[i].head; b = b->prev){
      if(b->refcnt == 0){
        if(i != h){
          // Remove from old bucket.
          b->next->prev = b->prev;
          b->prev->next = b->next;

          // Insert into target bucket.
          b->next = bcache.bucket[h].head.next;
          b->prev = &bcache.bucket[h].head;
          bcache.bucket[h].head.next->prev = b;
          bcache.bucket[h].head.next = b;
        }

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        if(i != h)
          release(&bcache.bucket[i].lock);
        release(&bcache.bucket[h].lock);
        release(&bcache.lock);

        acquiresleep(&b->lock);
        return b;
      }
    }

    if(i != h)
      release(&bcache.bucket[i].lock);
  }

  release(&bcache.bucket[h].lock);
  release(&bcache.lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  uint h;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  h = bhash(b->dev, b->blockno);
  acquire(&bcache.bucket[h].lock);
  b->refcnt--;
  release(&bcache.bucket[h].lock);
}

void
bpin(struct buf *b) {
  uint h = bhash(b->dev, b->blockno);

  acquire(&bcache.bucket[h].lock);
  b->refcnt++;
  release(&bcache.bucket[h].lock);
}

void
bunpin(struct buf *b) {
  uint h = bhash(b->dev, b->blockno);

  acquire(&bcache.bucket[h].lock);
  b->refcnt--;
  release(&bcache.bucket[h].lock);
}

