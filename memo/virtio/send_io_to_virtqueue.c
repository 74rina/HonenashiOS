// デバイスに新しいリクエストがあることを通知する。desc_indexは、新しいリクエストの
// 先頭ディスクリプタのインデックス。
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
  vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
  vq->avail.index++;
  __sync_synchronize();
  virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
  vq->last_used_index++;
}

// デバイスが処理中のリクエストがあるかどうかを返す。
bool virtq_is_busy(struct virtio_virtq *vq) {
  return vq->last_used_index != *vq->used_index;
}

// virtio-blkデバイスの読み書き。
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

/*
デバイスにセクタ単位で読み書きする方法

まず、リクエストの構造体virtio_blk_reqを作る
struct virtio_blk_req {
    uint32_t type;     // IN / OUT
    uint32_t reserved;
    uint64_t sector;   // どのセクタか
    uint8_t data[SECTOR_SIZE];
    uint8_t status;
};

それから、read_write_disk関数の第二引数sectorを使って
blk_req->sector = sector;
と設定して、

これがMMIOの本質。

CPU メモリ上:
+----------------+
| blk_req        | ← virtio_blk_req 構造体（sector番号、バッファアドレスなど）
+----------------+
| buf            | ← データ受け取り用の CPU バッファ
+----------------+

MMIO (0x10001000〜):
+----------------+
| QueueNotify    | ← CPU が書き込むことで virtio-blk に命令を通知
| DeviceStatus   |
| DeviceConfig   |
+----------------+

QEMU 内の仮想ディスク:
+----------------+
| lorem.txt の内容 |
+----------------+

*/