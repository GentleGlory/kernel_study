# PCI-X Capability
## Capability ID
- 0x07

## 結構格式
| Offset | 欄位名稱 | 說明 |
|--------|----------|------|
| 00h | Capability ID | 固定值 0x07 |
| 01h | Next Capability Pointer | 指向下一個 Capability 結構的偏移量 |
| 02h | PCI-X Command Register | PCI-X 命令暫存器 |
| 04h | PCI-X Status Register | PCI-X 狀態暫存器 |

## PCI-X Command Register (偏移量 02h-03h)
| 位元 | 欄位名稱 | 說明 |
|------|----------|------|
| 15-7 | Reserved | 保留位元 |
| 6-4 | Maximum Outstanding Split Transactions | 最大未完成分割交易數 |
| 3-2 | Maximum Memory Read Byte Count | 最大記憶體讀取位元組數 |
| 1 | Enable Relaxed Ordering | 啟用寬鬆排序 |
| 0 | Data Parity Error Recovery Enable | 資料奇偶校驗錯誤恢復啟用 |

## PCI-X Status Register (偏移量 04h-07h)
| 位元 | 位元遮罩 | 欄位名稱 | 說明 |
|------|----------|----------|------|
| 31 | 0x80000000 | 533 MHz Capable | 支援 533 MHz |
| 30 | 0x40000000 | 266 MHz Capable | 支援 266 MHz |
| 29 | 0x20000000 | Split Completion Error Message | 接收到分割完成錯誤訊息 |
| 28-26 | 0x1C000000 | Designed Max Cumulative Read Size | 設計最大累積讀取大小 |
| 25-23 | 0x03800000 | Designed Max Outstanding Split Transactions | 設計最大未完成分割交易數 |
| 22-21 | 0x00600000 | Designed Max Memory Read Count | 設計最大記憶體讀取計數 |
| 20 | 0x00100000 | Device Complexity | 裝置複雜度 (0=簡單裝置, 1=橋接器) |
| 19 | 0x00080000 | Unexpected Split Completion | 非預期的分割完成 |
| 18 | 0x00040000 | Split Completion Discarded | 分割完成被丟棄 |
| 17 | 0x00020000 | 133 MHz Capable | 支援 133 MHz |
| 16 | 0x00010000 | 64-bit Device | 64 位元裝置 |
| 15-8 | 0x0000FF00 | Bus Number | 匯流排編號 |
| 7-0 | 0x000000FF | Device and Function Number | 裝置和功能編號 (DevFn) |


# PCI Express Capability
## Capability ID
- 0x10

## 結構格式
| Offset | 欄位名稱                                            | 說明                                             |
| ------ | ----------------------------------------------- | ---------------------------------------------- |
| 0x00   | PCI Express Capability ID (0x10)                | 固定為 0x10                                       |
| 0x01   | Next Capability Pointer                         | 指向下一個 capability                               |
| 0x02   | PCI Express Capabilities Register               | 包含裝置/端點類型、版本號等                                 |
| 0x04   | Device Capabilities Register                    | 描述最大 payload、功能特性                              |
| 0x08   | Device Control Register                         | 控制 MPS、错误报告、Relaxed Ordering 等                 |
| 0x0A   | Device Status Register                          | 錯誤狀態、補充能力狀態                                    |
| 0x0C   | Link Capabilities Register                      | 描述鏈路速度 (2.5/5/8/16/32 GT/s)、寬度、ASPM 能力         |
| 0x10   | Link Control Register                           | 控制 ASPM、讀取鏈路狀態                                 |
| 0x12   | Link Status Register                            | 當前鏈路速度、寬度、訓練狀態                                 |
| 0x14   | Slot Capabilities Register (僅 Root Port/Slot 端) | Hot-plug、電源控制等                                 |
| 0x18   | Slot Control Register                           | 控制 hot-plug 相關功能                               |
| 0x1A   | Slot Status Register                            | 插槽狀態 (裝置插入、電源錯誤等)                              |
| 0x1C   | Root Capabilities Register (僅 Root Complex)     | AER、PME 相關能力                                   |
| 0x20   | Root Control Register                           | 控制 PME、錯誤訊號路由                                  |
| 0x24   | Root Status Register                            | 錯誤事件狀態                                         |
| 0x28   | Device Capabilities 2 Register                  | Gen3/4/5 新增功能 (TLP Processing, AtomicOps, LTR) |
| 0x2C   | Device Control 2 Register                       | MPS 相關控制、OBFF、AtomicOps 控制                     |
| 0x30   | Link Capabilities 2 Register                    | 新一代鏈路能力 (16 GT/s, 32 GT/s)                     |
| 0x34   | Link Control 2 Register                         | 速度選擇、鏈路訓練控制                                    |
| 0x36   | Link Status 2 Register                          | 當前速度狀態                                         |
| 0x38   | Slot Capabilities 2 Register                    | 擴展插槽能力                                         |
| 0x3C   | Slot Control 2 Register                         | 擴展插槽控制                                         |
| 0x3E   | Slot Status 2 Register                          | 擴展插槽狀態                                         |

> 註：長度取決於裝置支援的規格 (PCIe 1.0/2.0/3.0/4.0/5.0)，某些欄位在 Endpoint 上不存在 (例如 Slot Registers)。

### PCI Express Capabilities Register
| Bits  | 名稱                       | 說明                                                                        |
| ----- | ------------------------ | ------------------------------------------------------------------------- |
| 3:0   | Capability Version       | PCIe 規格版本 (1=PCIe 1.0, 2=PCIe 2.0 …)                                      |
| 7:4   | Device/Port Type         | 0 = PCIe Endpoint, 4 = Root Port, 5 = Upstream Port, 6 = Downstream Port… |
| 8     | Slot Implemented         | 該功能是否有 Slot Registers                                                     |
| 13:9  | Interrupt Message Number | MSI/MSI-X 訊息編號                                                            |
| 15:14 | Reserved                 | —                                                                         |



