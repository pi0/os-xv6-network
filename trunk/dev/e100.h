#ifndef JOS_DEV_E100_H
#define JOS_DEV_E100_H 1

#include "pci.h"

#define E100_MEMORY	0
#define E100_IO		1
#define E100_FLASH	2

#define E100_VENDOR	0x8086
#define E100_PRODUCT	0x1209

#define CSR_SCB		0x0
#define CSR_STATUS	0x0
#define CSR_US		0x0
#define CSR_STATACK	0x1
#define CSR_COMMAND	0x2
#define CSR_UC		0x2
#define CSR_INT		0x3
#define CSR_GP		0x4
#define CSR_PORT	0x8

#define CUS_MASK	0xc0
#define CUS_IDLE	0x0
#define CUS_SUSPENDED	0x1
#define CUS_LPQ_ACTIVE	0x2
#define CUS_HQP_ACTIVE	0x3

#define RUS_MASK	0x3c
#define RUS_IDLE	0x0
#define	RUS_SUSPEND	0x4
#define RUS_NORES	0x8
#define RUS_READY	0x10

#define CUC_SHIFT	0
#define CUC_NOP		0x0
#define CUC_START	0x10
#define CUC_RESUME	0x20
#define CUC_LOAD_BASE	0x60

#define RUC_NOP		0x0
#define RUC_START	0x1
#define RUC_RESUME	0x2
#define RUC_REDIR	0x3
#define RUC_ABORT	0x4
#define RUC_LOADHDS	0x5
#define RUC_LOAD_BASE	0x6

#define PORT_SW_RESET	0x0
#define PORT_SELF_TEST	0x1
#define PORT_SEL_RESET	0x2

#define TX_CBL_MAX	128
#define RX_RFA_MAX	128

#define ERR_CBL_FULL	0
#define ERR_CBL_EMPTY	1
#define ERR_RFA_FULL	2
#define ERR_RFA_EMPTY	3

#define TCB_MAX_SIZE	1518
#define CBF_EL		0x8000
#define CBF_S		0x4000
#define CBF_I		0x2000
#define CBC_NOP		0x0
#define CBC_IAS		0x1
#define CBC_CONFIG	0x2
#define CBC_MAS		0x3
#define CBC_TRANSMIT	0x4
#define CBS_F		0x0800
#define CBS_OK		0x2000
#define CBS_C		0x8000
#define THRS_DEFAULT	0xe0

#define RFD_MAX_SIZE	1518
#define RFDF_EL		0x8000
#define RFDF_S		0x4000
#define RFDF_H		0x10
#define RFDF_SF		0x8
#define RFDS_C		0x8000
#define RFDS_OK		0x2000
#define RFDS_MASK	0x1fff
#define RFD_SIZE_MASK	0x3fff
#define RFD_AC_MASK	0x3fff
#define RFD_EOF		0x8000
#define RFD_F		0x4000

struct dev_e100 {
	struct pci_bus *bus;
	
	uint32_t dev_id;
	uint32_t dev_class;

	uint32_t reg_base[6];
	uint32_t reg_size[6];
	uint8_t irq_line;
};

struct tcb_data_t {
	uint32_t tbd_array_addr;
	uint16_t tcb_size;
	uint16_t tbd_count_thrs;
	char data[TCB_MAX_SIZE];	
};

union cb_data_t {
	struct tcb_data_t tcb_data;
};

struct cb_t {
	volatile uint16_t cb_status;
	uint16_t cb_control;
	uint32_t cb_link;
	union cb_data_t cb_data;
};

struct cbl_t {
	struct cb_t *start;
	struct cb_t *next;
	struct cb_t *prev;
	uint32_t size;
};

struct rfd_t {
	volatile uint16_t rfd_status;
	uint16_t rfd_command;
	uint32_t rfd_link;
	uint32_t rfd_reserved;
	uint16_t rfd_datainfo;
	uint16_t rfd_size;
	char data[RFD_MAX_SIZE];
};

struct rfa_t {
	struct rfd_t *start;
	struct rfd_t *base;
	struct rfd_t *end;
	uint32_t size;
};

int e100_attach(struct pci_func *pcif);
int e100_sw_reset(struct dev_e100 *e100);
int e100_transmit(char *data, uint16_t size);
int e100_getdata(char *data, uint16_t *size);
#endif	// !JOS_DEV_E100_H
