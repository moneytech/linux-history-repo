/*
 * irq_comm.c: Common API for in kernel interrupt controller
 * Copyright (c) 2007, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 * Authors:
 *   Yaozu (Eddie) Dong <Eddie.dong@intel.com>
 *
 */

#include <linux/kvm_host.h>

#include <asm/msidef.h>

#include "irq.h"

#include "ioapic.h"

static int kvm_set_pic_irq(struct kvm_kernel_irq_routing_entry *e,
			   struct kvm *kvm, int level)
{
#ifdef CONFIG_X86
	return kvm_pic_set_irq(pic_irqchip(kvm), e->irqchip.pin, level);
#else
	return -1;
#endif
}

static int kvm_set_ioapic_irq(struct kvm_kernel_irq_routing_entry *e,
			      struct kvm *kvm, int level)
{
	return kvm_ioapic_set_irq(kvm->arch.vioapic, e->irqchip.pin, level);
}

void kvm_get_intr_delivery_bitmask(struct kvm_ioapic *ioapic,
				   union kvm_ioapic_redirect_entry *entry,
				   unsigned long *deliver_bitmask)
{
	int i;
	struct kvm *kvm = ioapic->kvm;
	struct kvm_vcpu *vcpu;

	bitmap_zero(deliver_bitmask, KVM_MAX_VCPUS);

	if (entry->fields.dest_mode == 0) {	/* Physical mode. */
		if (entry->fields.dest_id == 0xFF) {	/* Broadcast. */
			for (i = 0; i < KVM_MAX_VCPUS; ++i)
				if (kvm->vcpus[i] && kvm->vcpus[i]->arch.apic)
					__set_bit(i, deliver_bitmask);
			/* Lowest priority shouldn't combine with broadcast */
			if (entry->fields.delivery_mode ==
			    IOAPIC_LOWEST_PRIORITY && printk_ratelimit())
				printk(KERN_INFO "kvm: apic: phys broadcast "
						  "and lowest prio\n");
			return;
		}
		for (i = 0; i < KVM_MAX_VCPUS; ++i) {
			vcpu = kvm->vcpus[i];
			if (!vcpu)
				continue;
			if (kvm_apic_match_physical_addr(vcpu->arch.apic,
					entry->fields.dest_id)) {
				if (vcpu->arch.apic)
					__set_bit(i, deliver_bitmask);
				break;
			}
		}
	} else if (entry->fields.dest_id != 0) /* Logical mode, MDA non-zero. */
		for (i = 0; i < KVM_MAX_VCPUS; ++i) {
			vcpu = kvm->vcpus[i];
			if (!vcpu)
				continue;
			if (vcpu->arch.apic &&
			    kvm_apic_match_logical_addr(vcpu->arch.apic,
					entry->fields.dest_id))
				__set_bit(i, deliver_bitmask);
		}

	switch (entry->fields.delivery_mode) {
	case IOAPIC_LOWEST_PRIORITY:
		/* Select one in deliver_bitmask */
		vcpu = kvm_get_lowest_prio_vcpu(ioapic->kvm,
				entry->fields.vector, deliver_bitmask);
		bitmap_zero(deliver_bitmask, KVM_MAX_VCPUS);
		if (!vcpu)
			return;
		__set_bit(vcpu->vcpu_id, deliver_bitmask);
		break;
	case IOAPIC_FIXED:
	case IOAPIC_NMI:
		break;
	default:
		if (printk_ratelimit())
			printk(KERN_INFO "kvm: unsupported delivery mode %d\n",
				entry->fields.delivery_mode);
		bitmap_zero(deliver_bitmask, KVM_MAX_VCPUS);
	}
}

static int kvm_set_msi(struct kvm_kernel_irq_routing_entry *e,
		       struct kvm *kvm, int level)
{
	int vcpu_id, r = -1;
	struct kvm_vcpu *vcpu;
	struct kvm_ioapic *ioapic = ioapic_irqchip(kvm);
	union kvm_ioapic_redirect_entry entry;
	DECLARE_BITMAP(deliver_bitmask, KVM_MAX_VCPUS);

	BUG_ON(!ioapic);

	entry.bits = 0;
	entry.fields.dest_id = (e->msi.address_lo &
			MSI_ADDR_DEST_ID_MASK) >> MSI_ADDR_DEST_ID_SHIFT;
	entry.fields.vector = (e->msi.data &
			MSI_DATA_VECTOR_MASK) >> MSI_DATA_VECTOR_SHIFT;
	entry.fields.dest_mode = test_bit(MSI_ADDR_DEST_MODE_SHIFT,
			(unsigned long *)&e->msi.address_lo);
	entry.fields.trig_mode = test_bit(MSI_DATA_TRIGGER_SHIFT,
			(unsigned long *)&e->msi.data);
	entry.fields.delivery_mode = test_bit(
			MSI_DATA_DELIVERY_MODE_SHIFT,
			(unsigned long *)&e->msi.data);

	/* TODO Deal with RH bit of MSI message address */

	kvm_get_intr_delivery_bitmask(ioapic, &entry, deliver_bitmask);

	if (find_first_bit(deliver_bitmask, KVM_MAX_VCPUS) >= KVM_MAX_VCPUS) {
		printk(KERN_WARNING "kvm: no destination for MSI delivery!");
		return -1;
	}
	while ((vcpu_id = find_first_bit(deliver_bitmask,
					KVM_MAX_VCPUS)) < KVM_MAX_VCPUS) {
		__clear_bit(vcpu_id, deliver_bitmask);
		vcpu = ioapic->kvm->vcpus[vcpu_id];
		if (vcpu) {
			if (r < 0)
				r = 0;
			r += kvm_apic_set_irq(vcpu, entry.fields.vector,
					      entry.fields.trig_mode);
		}
	}
	return r;
}

/* This should be called with the kvm->lock mutex held
 * Return value:
 *  < 0   Interrupt was ignored (masked or not delivered for other reasons)
 *  = 0   Interrupt was coalesced (previous irq is still pending)
 *  > 0   Number of CPUs interrupt was delivered to
 */
int kvm_set_irq(struct kvm *kvm, int irq_source_id, int irq, int level)
{
	struct kvm_kernel_irq_routing_entry *e;
	unsigned long *irq_state, sig_level;
	int ret = -1;

	if (irq < KVM_IOAPIC_NUM_PINS) {
		irq_state = (unsigned long *)&kvm->arch.irq_states[irq];

		/* Logical OR for level trig interrupt */
		if (level)
			set_bit(irq_source_id, irq_state);
		else
			clear_bit(irq_source_id, irq_state);
		sig_level = !!(*irq_state);
	} else /* Deal with MSI/MSI-X */
		sig_level = 1;

	/* Not possible to detect if the guest uses the PIC or the
	 * IOAPIC.  So set the bit in both. The guest will ignore
	 * writes to the unused one.
	 */
	list_for_each_entry(e, &kvm->irq_routing, link)
		if (e->gsi == irq) {
			int r = e->set(e, kvm, sig_level);
			if (r < 0)
				continue;

			ret = r + ((ret < 0) ? 0 : ret);
		}
	return ret;
}

void kvm_notify_acked_irq(struct kvm *kvm, unsigned irqchip, unsigned pin)
{
	struct kvm_kernel_irq_routing_entry *e;
	struct kvm_irq_ack_notifier *kian;
	struct hlist_node *n;
	unsigned gsi = pin;

	list_for_each_entry(e, &kvm->irq_routing, link)
		if (e->irqchip.irqchip == irqchip &&
		    e->irqchip.pin == pin) {
			gsi = e->gsi;
			break;
		}

	hlist_for_each_entry(kian, n, &kvm->arch.irq_ack_notifier_list, link)
		if (kian->gsi == gsi)
			kian->irq_acked(kian);
}

void kvm_register_irq_ack_notifier(struct kvm *kvm,
				   struct kvm_irq_ack_notifier *kian)
{
	hlist_add_head(&kian->link, &kvm->arch.irq_ack_notifier_list);
}

void kvm_unregister_irq_ack_notifier(struct kvm_irq_ack_notifier *kian)
{
	hlist_del_init(&kian->link);
}

/* The caller must hold kvm->lock mutex */
int kvm_request_irq_source_id(struct kvm *kvm)
{
	unsigned long *bitmap = &kvm->arch.irq_sources_bitmap;
	int irq_source_id = find_first_zero_bit(bitmap,
				sizeof(kvm->arch.irq_sources_bitmap));

	if (irq_source_id >= sizeof(kvm->arch.irq_sources_bitmap)) {
		printk(KERN_WARNING "kvm: exhaust allocatable IRQ sources!\n");
		return -EFAULT;
	}

	ASSERT(irq_source_id != KVM_USERSPACE_IRQ_SOURCE_ID);
	set_bit(irq_source_id, bitmap);

	return irq_source_id;
}

void kvm_free_irq_source_id(struct kvm *kvm, int irq_source_id)
{
	int i;

	ASSERT(irq_source_id != KVM_USERSPACE_IRQ_SOURCE_ID);

	if (irq_source_id < 0 ||
	    irq_source_id >= sizeof(kvm->arch.irq_sources_bitmap)) {
		printk(KERN_ERR "kvm: IRQ source ID out of range!\n");
		return;
	}
	for (i = 0; i < KVM_IOAPIC_NUM_PINS; i++)
		clear_bit(irq_source_id, &kvm->arch.irq_states[i]);
	clear_bit(irq_source_id, &kvm->arch.irq_sources_bitmap);
}

void kvm_register_irq_mask_notifier(struct kvm *kvm, int irq,
				    struct kvm_irq_mask_notifier *kimn)
{
	kimn->irq = irq;
	hlist_add_head(&kimn->link, &kvm->mask_notifier_list);
}

void kvm_unregister_irq_mask_notifier(struct kvm *kvm, int irq,
				      struct kvm_irq_mask_notifier *kimn)
{
	hlist_del(&kimn->link);
}

void kvm_fire_mask_notifiers(struct kvm *kvm, int irq, bool mask)
{
	struct kvm_irq_mask_notifier *kimn;
	struct hlist_node *n;

	hlist_for_each_entry(kimn, n, &kvm->mask_notifier_list, link)
		if (kimn->irq == irq)
			kimn->func(kimn, mask);
}

static void __kvm_free_irq_routing(struct list_head *irq_routing)
{
	struct kvm_kernel_irq_routing_entry *e, *n;

	list_for_each_entry_safe(e, n, irq_routing, link)
		kfree(e);
}

void kvm_free_irq_routing(struct kvm *kvm)
{
	__kvm_free_irq_routing(&kvm->irq_routing);
}

static int setup_routing_entry(struct kvm_kernel_irq_routing_entry *e,
			       const struct kvm_irq_routing_entry *ue)
{
	int r = -EINVAL;
	int delta;

	e->gsi = ue->gsi;
	switch (ue->type) {
	case KVM_IRQ_ROUTING_IRQCHIP:
		delta = 0;
		switch (ue->u.irqchip.irqchip) {
		case KVM_IRQCHIP_PIC_MASTER:
			e->set = kvm_set_pic_irq;
			break;
		case KVM_IRQCHIP_PIC_SLAVE:
			e->set = kvm_set_pic_irq;
			delta = 8;
			break;
		case KVM_IRQCHIP_IOAPIC:
				e->set = kvm_set_ioapic_irq;
			break;
		default:
			goto out;
		}
		e->irqchip.irqchip = ue->u.irqchip.irqchip;
		e->irqchip.pin = ue->u.irqchip.pin + delta;
		break;
	case KVM_IRQ_ROUTING_MSI:
		e->set = kvm_set_msi;
		e->msi.address_lo = ue->u.msi.address_lo;
		e->msi.address_hi = ue->u.msi.address_hi;
		e->msi.data = ue->u.msi.data;
		break;
	default:
		goto out;
	}
	r = 0;
out:
	return r;
}


int kvm_set_irq_routing(struct kvm *kvm,
			const struct kvm_irq_routing_entry *ue,
			unsigned nr,
			unsigned flags)
{
	struct list_head irq_list = LIST_HEAD_INIT(irq_list);
	struct list_head tmp = LIST_HEAD_INIT(tmp);
	struct kvm_kernel_irq_routing_entry *e = NULL;
	unsigned i;
	int r;

	for (i = 0; i < nr; ++i) {
		r = -EINVAL;
		if (ue->gsi >= KVM_MAX_IRQ_ROUTES)
			goto out;
		if (ue->flags)
			goto out;
		r = -ENOMEM;
		e = kzalloc(sizeof(*e), GFP_KERNEL);
		if (!e)
			goto out;
		r = setup_routing_entry(e, ue);
		if (r)
			goto out;
		++ue;
		list_add(&e->link, &irq_list);
		e = NULL;
	}

	mutex_lock(&kvm->lock);
	list_splice(&kvm->irq_routing, &tmp);
	INIT_LIST_HEAD(&kvm->irq_routing);
	list_splice(&irq_list, &kvm->irq_routing);
	INIT_LIST_HEAD(&irq_list);
	list_splice(&tmp, &irq_list);
	mutex_unlock(&kvm->lock);

	r = 0;

out:
	kfree(e);
	__kvm_free_irq_routing(&irq_list);
	return r;
}

#define IOAPIC_ROUTING_ENTRY(irq) \
	{ .gsi = irq, .type = KVM_IRQ_ROUTING_IRQCHIP,	\
	  .u.irqchip.irqchip = KVM_IRQCHIP_IOAPIC, .u.irqchip.pin = (irq) }
#define ROUTING_ENTRY1(irq) IOAPIC_ROUTING_ENTRY(irq)

#ifdef CONFIG_X86
#  define PIC_ROUTING_ENTRY(irq) \
	{ .gsi = irq, .type = KVM_IRQ_ROUTING_IRQCHIP,	\
	  .u.irqchip.irqchip = SELECT_PIC(irq), .u.irqchip.pin = (irq) % 8 }
#  define ROUTING_ENTRY2(irq) \
	IOAPIC_ROUTING_ENTRY(irq), PIC_ROUTING_ENTRY(irq)
#else
#  define ROUTING_ENTRY2(irq) \
	IOAPIC_ROUTING_ENTRY(irq)
#endif

static const struct kvm_irq_routing_entry default_routing[] = {
	ROUTING_ENTRY2(0), ROUTING_ENTRY2(1),
	ROUTING_ENTRY2(2), ROUTING_ENTRY2(3),
	ROUTING_ENTRY2(4), ROUTING_ENTRY2(5),
	ROUTING_ENTRY2(6), ROUTING_ENTRY2(7),
	ROUTING_ENTRY2(8), ROUTING_ENTRY2(9),
	ROUTING_ENTRY2(10), ROUTING_ENTRY2(11),
	ROUTING_ENTRY2(12), ROUTING_ENTRY2(13),
	ROUTING_ENTRY2(14), ROUTING_ENTRY2(15),
	ROUTING_ENTRY1(16), ROUTING_ENTRY1(17),
	ROUTING_ENTRY1(18), ROUTING_ENTRY1(19),
	ROUTING_ENTRY1(20), ROUTING_ENTRY1(21),
	ROUTING_ENTRY1(22), ROUTING_ENTRY1(23),
#ifdef CONFIG_IA64
	ROUTING_ENTRY1(24), ROUTING_ENTRY1(25),
	ROUTING_ENTRY1(26), ROUTING_ENTRY1(27),
	ROUTING_ENTRY1(28), ROUTING_ENTRY1(29),
	ROUTING_ENTRY1(30), ROUTING_ENTRY1(31),
	ROUTING_ENTRY1(32), ROUTING_ENTRY1(33),
	ROUTING_ENTRY1(34), ROUTING_ENTRY1(35),
	ROUTING_ENTRY1(36), ROUTING_ENTRY1(37),
	ROUTING_ENTRY1(38), ROUTING_ENTRY1(39),
	ROUTING_ENTRY1(40), ROUTING_ENTRY1(41),
	ROUTING_ENTRY1(42), ROUTING_ENTRY1(43),
	ROUTING_ENTRY1(44), ROUTING_ENTRY1(45),
	ROUTING_ENTRY1(46), ROUTING_ENTRY1(47),
#endif
};

int kvm_setup_default_irq_routing(struct kvm *kvm)
{
	return kvm_set_irq_routing(kvm, default_routing,
				   ARRAY_SIZE(default_routing), 0);
}
