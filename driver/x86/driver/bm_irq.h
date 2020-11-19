#ifndef _BM_IRQ_H_
#define _BM_IRQ_H_

#ifndef SOC_MODE
struct bm_device_info;
#define MSG_IRQ_ID_CHANNEL_XPU	48
#define MSG_IRQ_ID_CHANNEL_CPU  49
#define CDMA_IRQ_ID	46
typedef void (*bmdrv_submodule_irq_handler)(struct bm_device_info *bmdi);
extern bmdrv_submodule_irq_handler bmdrv_module_irq_handler[128];
void bmdrv_enable_irq(struct bm_device_info *bmdi, int irq_num);
void bmdrv_disable_irq(struct bm_device_info *bmdi, int irq_num);
void bmdrv_submodule_request_irq(struct bm_device_info *bmdi, int irq_num,
			bmdrv_submodule_irq_handler irq_handler);
void bmdrv_submodule_free_irq(struct bm_device_info *bmdi, int irq_num);
int bmdrv_init_irq(struct pci_dev *pdev);
void bmdrv_free_irq(struct pci_dev *pdev);
#else
#include <linux/platform_device.h>
int bmdrv_init_irq(struct platform_device *pdev);
void bmdrv_free_irq(struct platform_device *pdev);
#endif
#endif
