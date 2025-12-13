#include "./virtio.h"
#include "../kernel.h"

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
uint64_t blk_capacity;

uint32_t virtio_reg_read32(unsigned offset) {
  return *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned offset) {
  return *((volatile uint64_t *)(VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, uint32_t value) {
  *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
  virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

struct virtio_virtq *virtq_init(unsigned index) {
  paddr_t virtq_paddr =
      alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
  struct virtio_virtq *vq = (struct virtio_virtq *)virtq_paddr;
  vq->queue_index = index;
  vq->used_index = (volatile uint16_t *)&vq->used.index;
  // 1. Select the queue writing its index (first queue is 0) to QueueSel.
  virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
  // 5. Notify the device about the queue size by writing the size to QueueNum.
  virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
  // 6. Notify the device about the used alignment by writing its value in bytes
  // to QueueAlign.
  virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
  // 7. Write the physical number of the first page of the queue to the QueuePFN
  // register.
  virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
  return vq;
}

void virtio_blk_init(void) {
  if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
    PANIC("virtio: invalid magic value");
  if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
    PANIC("virtio: invalid version");
  if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
    PANIC("virtio: invalid device id");

  // 1. Reset the device.
  virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
  // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
  virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
  // 3. Set the DRIVER status bit.
  virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
  // 5. Set the FEATURES_OK status bit.
  virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);
  // 7. Perform device-specific setup, including discovery of virtqueues for the
  // device
  blk_request_vq = virtq_init(0);
  // 8. Set the DRIVER_OK status bit.
  virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

  // ディスクの容量を取得
  blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
  printf("virtio-blk: capacity is %d bytes\n", blk_capacity);

  // デバイスへの処理要求を格納する領域を確保
  blk_req_paddr =
      alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
  blk_req = (struct virtio_blk_req *)blk_req_paddr;
}

// send to virtqueue
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
  vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
  vq->avail.index++;
  __sync_synchronize();
  virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
  vq->last_used_index++;
}

bool virtq_is_busy(struct virtio_virtq *vq) {
  return vq->last_used_index != *vq->used_index;
}

void read_write_disk(void *buf, unsigned sector, int is_write) {
  if (sector >= blk_capacity / SECTOR_SIZE) {
    printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
           sector, blk_capacity / SECTOR_SIZE);
    return;
  }

  // virtio-blkの仕様に従って、リクエストを構築する
  blk_req->sector = sector;
  blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
  if (is_write)
    memcpy(blk_req->data, buf, SECTOR_SIZE);

  // virtqueueのディスクリプタを構築する (3つのディスクリプタを使う)
  struct virtio_virtq *vq = blk_request_vq;
  vq->descs[0].addr = blk_req_paddr;
  vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
  vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
  vq->descs[0].next = 1;

  vq->descs[1].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
  vq->descs[1].len = SECTOR_SIZE;
  vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
  vq->descs[1].next = 2;

  vq->descs[2].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
  vq->descs[2].len = sizeof(uint8_t);
  vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

  // デバイスに新しいリクエストがあることを通知する
  virtq_kick(vq, 0);

  // デバイス側の処理が終わるまで待つ
  while (virtq_is_busy(vq))
    ;

  // virtio-blk: 0でない値が返ってきたらエラー
  if (blk_req->status != 0) {
    printf("virtio: warn: failed to read/write sector=%d status=%d\n", sector,
           blk_req->status);
    return;
  }

  // 読み込み処理の場合は、バッファにデータをコピーする
  if (!is_write)
    memcpy(buf, blk_req->data, SECTOR_SIZE);
}