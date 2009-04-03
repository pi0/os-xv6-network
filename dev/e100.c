// LAB 6: Your driver code here

#include "e100.h"
#include "pci.h"

#include <x86.h>
#include <debug.h>
#include <defs.h>
// #include <inc/memlayout.h>

struct dev_e100 e100;

struct cb_t tx_cbl_array[TX_CBL_MAX];
struct rfd_t rx_rfa_array[RX_RFA_MAX];
struct cbl_t tx_cbl;
struct rfa_t rx_rfa;

// inline void
// e100_cu_base_addr(struct dev_e100 *e100, uint32_t base_addr) {

void tx_cbl_init();
void rx_rfa_init();
int tx_cbl_append_nop(uint16_t flag);
int tx_cbl_append_tx(char *data, uint16_t size, uint16_t flag);
void e100_init(struct dev_e100 *e100);

int
e100_attach(struct pci_func *pcif) {
	pci_func_enable(pcif);
	e100.bus = pcif->bus;
	e100.dev_id = pcif->dev_id;
	e100.dev_class = pcif->dev_class;
	int i;
	for (i = 0; i < 3; i++) {
		e100.reg_base[i] = pcif->reg_base[i];
		e100.reg_size[i] = pcif->reg_size[i];
	}
	e100.irq_line = pcif->irq_line;

	return e100_sw_reset(&e100);
}

int
e100_sw_reset(struct dev_e100 *e100) {
	outl(e100->reg_base[E100_IO] + CSR_PORT, PORT_SW_RESET);
	
	// delay about 10us
	int i = 0;
	for (i = 0; i < 8; i++) {
		inb (0x84);
	}

	e100_init(e100);

	return 1;
}

void
e100_init(struct dev_e100 *e100) {
	tx_cbl_init();
	rx_rfa_init();
	
	// use linear addressing
	uint16_t scb_status;
	char  scb_command;
	uint32_t io_base;
	io_base = e100->reg_base[E100_IO];
	
	// Do not generate interrupts
	outb(io_base + CSR_INT, 1);
	do {
	 	scb_command = inb(io_base + CSR_COMMAND);
	} while (scb_command != 0);
	
	// initialize CU
	do {
		scb_status = inb(io_base + CSR_STATUS);
	} while ((scb_status & CUS_MASK) != CUS_IDLE);
	outl(io_base + CSR_GP, 0);
	outb(io_base + CSR_COMMAND, CUC_LOAD_BASE);
	do {
		scb_command = inb(io_base + CSR_COMMAND);
	} while (scb_command != 0);	
	tx_cbl_append_nop(CBF_S);
	outl(io_base + CSR_GP, PADDR(tx_cbl.start));
	outb(io_base + CSR_COMMAND, CUC_START);
	do {
		scb_command = inb(io_base + CSR_COMMAND);
	} while (scb_command != 0);

	// initialize RU
	do {
		scb_status = inb(io_base + CSR_STATUS);
	} while ((scb_status & RUS_MASK) != RUS_IDLE);
	outl(io_base + CSR_GP, /* KERNBASE */ 0);
	outb(io_base + CSR_COMMAND, RUC_LOAD_BASE);
	do {
		scb_command = inb(io_base + CSR_COMMAND);
	} while (scb_command != 0);
	outl(io_base + CSR_GP, (uint32_t) PADDR(rx_rfa.start));
	outb(io_base + CSR_COMMAND, RUC_START);
	do {
		scb_command = inb(io_base + CSR_COMMAND);
	} while (scb_command != 0);
}

void
rfd_reset(struct rfd_t *ptr) {
	ptr->rfd_status = 0;
	ptr->rfd_datainfo = 0;
}

int 
e100_getdata(char *data, uint16_t *size) {
	uint16_t scb_command;
	uint32_t io_base = e100.reg_base[E100_IO];
	while (((rx_rfa.base->rfd_status & RFDS_C) != 0)
		&& ((rx_rfa.base->rfd_status & RFDS_OK) == 0)) {
		rfd_reset(rx_rfa.base);
		rx_rfa.base = KADDR(/*(struct rfd_t *)*/ rx_rfa.base->rfd_link);
	}
	if ((rx_rfa.base->rfd_status & (RFDS_C))
			== (RFDS_C)) {
		struct rfd_t *ptr = rx_rfa.base;
		char *data_p = data;
		uint16_t size0 = 0;
		// while ((ptr->rfd_datainfo & RFD_EOF) != 0) {
		//	memmove(data_p, ptr->data, RFD_MAX_SIZE);
		//	size0 += RFD_MAX_SIZE;
		//	ptr->rfd_datainfo = 0;
		//	ptr->rfd_status = 0;
		//	data_p += RFD_MAX_SIZE;
		//	ptr = KADDR(/*(struct rfd_t *)*/ ptr->rfd_link);
		//}
		*size = ptr->rfd_datainfo & RFD_AC_MASK;
		/*
		if (net_debug) {
			cprintf("received a packet of size: %d\n", *size);
			int i = 0;
			for (i = 0; i < *size; i++) {
				cprintf("%02x ", ptr->data[i]);
			}
			cprintf("\n");
		}
		*/
		memmove(data_p, ptr->data, *size - size0);
		ptr->rfd_datainfo = 0;
		ptr->rfd_status = 0;
		rx_rfa.base = KADDR(/*(struct rfd_t *)*/ ptr->rfd_link);
		ptr->rfd_command |= RFDF_EL;
		if (ptr != rx_rfa.end) {
			rx_rfa.end->rfd_command &= ~RFDF_EL;
			rx_rfa.end = ptr;
		}
		return 0;
	} else {
		return -ERR_RFA_EMPTY;
	}
}

int
e100_transmit(/* struct dev_e100 *e100, */ char *data, uint16_t size) {
	if (net_debug) {
		cprintf("transmit: enter\n");
	}
	uint16_t scb_command;
	uint32_t io_base = e100.reg_base[E100_IO];
	while ((tx_cbl.prev != tx_cbl.next)
			&& ((tx_cbl.prev->cb_status & CBS_C) != 0)) {
		// tx_cbl.prev->cb_status = 0;
		// tx_cbl.prev->cb_control = 0;
		tx_cbl.prev++;
		if (tx_cbl.prev == tx_cbl.start + tx_cbl.size)
			tx_cbl.prev = tx_cbl.start;
	}
	// if (tx_cbl.prev == tx_cbl.start) {
	//	tx_cbl_append_tx(data, size, CUC_S);
	//	outw(io_base + CSR_COMMAND, CUC_START);
	// } else {
		struct cb_t *cb_p = tx_cbl.next;
		if (((tx_cbl.next - tx_cbl.start + 1) % tx_cbl.size) == (tx_cbl.prev - tx_cbl.start)) {
			return -ERR_CBL_FULL;
		}
		tx_cbl_append_tx(data, size, CBF_S);
		if (cb_p == tx_cbl.start) {
			cb_p = tx_cbl.start + tx_cbl.size - 1;
		} else {
			cb_p--;
		}
		cb_p->cb_control &= ~CBF_S;
		outb(io_base + CSR_COMMAND, CUC_RESUME);
	do {
		scb_command = inb(io_base + CSR_COMMAND);
	} while (scb_command != 0);
	// }
	return 0;
	
}

int
tx_cbl_append_nop(uint16_t flag) {
	struct cb_t *next = tx_cbl.next;
	struct cb_t *start = tx_cbl.start;
	struct cb_t *prev = tx_cbl.prev;
	next->cb_status = 0;
	next->cb_control = CBC_NOP | flag;
	if ((next - start) < tx_cbl.size) {
		tx_cbl.next++;
	} else {
		tx_cbl.next = start;
	}
	return 0;
}

int
tx_cbl_append_tx(char *data, uint16_t size, uint16_t flag) {
	struct cb_t *next = tx_cbl.next;
	struct cb_t *start = tx_cbl.start;
	struct cb_t *prev = tx_cbl.prev;
	// if (((next - start + 1) % tx_cbl.size) == (prev - start)) {
	//	return -ERR_CBL_FULL;
	//}
	next->cb_status = 0;
	next->cb_control = CBC_TRANSMIT | flag;
	next->cb_data.tcb_data.tbd_array_addr = 0xffffffff;
	next->cb_data.tcb_data.tcb_size = size;
	next->cb_data.tcb_data.tbd_count_thrs = THRS_DEFAULT;
	memmove(next->cb_data.tcb_data.data, data, size);
	if ((next - start) < tx_cbl.size) {
		tx_cbl.next++;
	} else {
		tx_cbl.next = start;
	}
	return 0;
}

void
tx_cbl_init() {
	tx_cbl.start = tx_cbl_array;
	tx_cbl.size = TX_CBL_MAX;
	tx_cbl.next = tx_cbl_array;
	tx_cbl.prev = tx_cbl_array;
	int i = 0;
	memset(tx_cbl_array, 0, TX_CBL_MAX * sizeof(struct cb_t));
	for (i = 0; i < TX_CBL_MAX - 1; i++) {
		tx_cbl.start[i].cb_link = PADDR((uint32_t ) &(tx_cbl.start[i+1]));
	}
	tx_cbl.start[TX_CBL_MAX - 1].cb_link = PADDR((uint32_t) tx_cbl_array);
}

void
rx_rfa_init() {
	rx_rfa.start = rx_rfa_array;
	rx_rfa.size = RX_RFA_MAX;
	rx_rfa.base = rx_rfa_array;
	rx_rfa.end = rx_rfa_array + RX_RFA_MAX - 1;
	int i = 0;
	memset(rx_rfa_array, 0, RX_RFA_MAX * sizeof(struct rfd_t));
	for (i = 0; i < RX_RFA_MAX - 1; i++) {
		rx_rfa.start[i].rfd_link = PADDR((uint32_t) &(rx_rfa.start[i+1]));
		rx_rfa.start[i].rfd_size = RFD_MAX_SIZE & RFD_SIZE_MASK;
		rx_rfa.start[i].rfd_reserved = 0xffffffff;
	}
	rx_rfa.end->rfd_link = PADDR((uint32_t) rx_rfa_array);
	rx_rfa.end->rfd_command = RFDF_EL;
}

