/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Non-volatile storage routines.
 */
#include "sysincludes.h"


#include "crc8.h"
#include "utility.h"
#include "rollback_index.h"
#include "vboot_common.h"
#include "vboot_nvstorage.h"

/*
 * Constants for NV storage.  We use this rather than structs and bitfields so
 * the data format is consistent across platforms and compilers.
 */
#define HEADER_OFFSET                0
#define HEADER_MASK                     0xC0
#define HEADER_SIGNATURE                0x40
#define HEADER_FIRMWARE_SETTINGS_RESET  0x20
#define HEADER_KERNEL_SETTINGS_RESET    0x10

#define BOOT_OFFSET                  1
#define BOOT_DEBUG_RESET_MODE           0x80
#define BOOT_DISABLE_DEV_REQUEST        0x40
#define BOOT_OPROM_NEEDED               0x20
#define BOOT_BACKUP_NVRAM               0x10
#define BOOT_TRY_B_COUNT_MASK           0x0F

#define RECOVERY_OFFSET              2
#define LOCALIZATION_OFFSET          3

#define DEV_FLAGS_OFFSET             4
#define DEV_BOOT_USB_MASK               0x01
#define DEV_BOOT_SIGNED_ONLY_MASK       0x02
#define DEV_BOOT_LEGACY_MASK            0x04

#define TPM_FLAGS_OFFSET             5
#define TPM_CLEAR_OWNER_REQUEST         0x01
#define TPM_CLEAR_OWNER_DONE            0x02

#define RECOVERY_SUBCODE_OFFSET      6

#define KERNEL_FIELD_OFFSET         11
#define CRC_OFFSET                  15

int VbNvSetup(VbNvContext *context)
{
	uint8_t *raw = context->raw;

	/* Nothing has changed yet. */
	context->raw_changed = 0;
	context->regenerate_crc = 0;

	/* Check data for consistency */
	if ((HEADER_SIGNATURE != (raw[HEADER_OFFSET] & HEADER_MASK))
	    || (Crc8(raw, CRC_OFFSET) != raw[CRC_OFFSET])) {
		/* Data is inconsistent (bad CRC or header); reset defaults */
		Memset(raw, 0, VBNV_BLOCK_SIZE);
		raw[HEADER_OFFSET] = (HEADER_SIGNATURE |
				      HEADER_FIRMWARE_SETTINGS_RESET |
				      HEADER_KERNEL_SETTINGS_RESET);

		/* Regenerate CRC on exit */
		context->regenerate_crc = 1;
	}

	return 0;
}

int VbNvTeardown(VbNvContext *context)
{
	if (context->regenerate_crc) {
		context->raw[CRC_OFFSET] = Crc8(context->raw, CRC_OFFSET);
		context->regenerate_crc = 0;
		context->raw_changed = 1;
	}

	return 0;
}

int VbNvGet(VbNvContext *context, VbNvParam param, uint32_t *dest)
{
	const uint8_t *raw = context->raw;

	switch (param) {
	case VBNV_FIRMWARE_SETTINGS_RESET:
		*dest = (raw[HEADER_OFFSET] & HEADER_FIRMWARE_SETTINGS_RESET ?
			 1 : 0);
		return 0;

	case VBNV_KERNEL_SETTINGS_RESET:
		*dest = (raw[HEADER_OFFSET] & HEADER_KERNEL_SETTINGS_RESET ?
			 1 : 0);
		return 0;

	case VBNV_DEBUG_RESET_MODE:
		*dest = (raw[BOOT_OFFSET] & BOOT_DEBUG_RESET_MODE ? 1 : 0);
		return 0;

	case VBNV_TRY_B_COUNT:
		*dest = raw[BOOT_OFFSET] & BOOT_TRY_B_COUNT_MASK;
		return 0;

	case VBNV_RECOVERY_REQUEST:
		*dest = raw[RECOVERY_OFFSET];
		return 0;

	case VBNV_RECOVERY_SUBCODE:
		*dest = raw[RECOVERY_SUBCODE_OFFSET];
		return 0;

	case VBNV_LOCALIZATION_INDEX:
		*dest = raw[LOCALIZATION_OFFSET];
		return 0;

	case VBNV_KERNEL_FIELD:
		*dest = (raw[KERNEL_FIELD_OFFSET]
			 | (raw[KERNEL_FIELD_OFFSET + 1] << 8)
			 | (raw[KERNEL_FIELD_OFFSET + 2] << 16)
			 | (raw[KERNEL_FIELD_OFFSET + 3] << 24));
		return 0;

	case VBNV_DEV_BOOT_USB:
		*dest = (raw[DEV_FLAGS_OFFSET] & DEV_BOOT_USB_MASK ? 1 : 0);
		return 0;

	case VBNV_DEV_BOOT_LEGACY:
		*dest = (raw[DEV_FLAGS_OFFSET] & DEV_BOOT_LEGACY_MASK ? 1 : 0);
		return 0;

	case VBNV_DEV_BOOT_SIGNED_ONLY:
		*dest = (raw[DEV_FLAGS_OFFSET] & DEV_BOOT_SIGNED_ONLY_MASK ?
			 1 : 0);
		return 0;

	case VBNV_DISABLE_DEV_REQUEST:
		*dest = (raw[BOOT_OFFSET] & BOOT_DISABLE_DEV_REQUEST ? 1 : 0);
		return 0;

	case VBNV_OPROM_NEEDED:
		*dest = (raw[BOOT_OFFSET] & BOOT_OPROM_NEEDED ? 1 : 0);
		return 0;

	case VBNV_CLEAR_TPM_OWNER_REQUEST:
		*dest = (raw[TPM_FLAGS_OFFSET] & TPM_CLEAR_OWNER_REQUEST ?
			 1 : 0);
		return 0;

	case VBNV_CLEAR_TPM_OWNER_DONE:
		*dest = (raw[TPM_FLAGS_OFFSET] & TPM_CLEAR_OWNER_DONE ? 1 : 0);
		return 0;

	case VBNV_BACKUP_NVRAM_REQUEST:
		*dest = (raw[BOOT_OFFSET] & BOOT_BACKUP_NVRAM ? 1 : 0);
		return 0;

	default:
		return 1;
	}
}

int VbNvSet(VbNvContext *context, VbNvParam param, uint32_t value)
{
	uint8_t *raw = context->raw;
	uint32_t current;

	/* If not changing the value, don't regenerate the CRC. */
	if (0 == VbNvGet(context, param, &current) && current == value)
		return 0;

	switch (param) {
	case VBNV_FIRMWARE_SETTINGS_RESET:
		if (value)
			raw[HEADER_OFFSET] |= HEADER_FIRMWARE_SETTINGS_RESET;
		else
			raw[HEADER_OFFSET] &= ~HEADER_FIRMWARE_SETTINGS_RESET;
		break;

	case VBNV_KERNEL_SETTINGS_RESET:
		if (value)
			raw[HEADER_OFFSET] |= HEADER_KERNEL_SETTINGS_RESET;
		else
			raw[HEADER_OFFSET] &= ~HEADER_KERNEL_SETTINGS_RESET;
		break;

	case VBNV_DEBUG_RESET_MODE:
		if (value)
			raw[BOOT_OFFSET] |= BOOT_DEBUG_RESET_MODE;
		else
			raw[BOOT_OFFSET] &= ~BOOT_DEBUG_RESET_MODE;
		break;

	case VBNV_TRY_B_COUNT:
		/* Clip to valid range. */
		if (value > BOOT_TRY_B_COUNT_MASK)
			value = BOOT_TRY_B_COUNT_MASK;

		raw[BOOT_OFFSET] &= ~BOOT_TRY_B_COUNT_MASK;
		raw[BOOT_OFFSET] |= (uint8_t)value;
		break;

	case VBNV_RECOVERY_REQUEST:
		/*
		 * Map values outside the valid range to the legacy reason,
		 * since we can't determine if we're called from kernel or user
		 * mode.
		 */
		if (value > 0xFF)
			value = VBNV_RECOVERY_LEGACY;
		raw[RECOVERY_OFFSET] = (uint8_t)value;
		break;

	case VBNV_RECOVERY_SUBCODE:
		raw[RECOVERY_SUBCODE_OFFSET] = (uint8_t)value;
		break;

	case VBNV_LOCALIZATION_INDEX:
		/* Map values outside the valid range to the default index. */
		if (value > 0xFF)
			value = 0;
		raw[LOCALIZATION_OFFSET] = (uint8_t)value;
		break;

	case VBNV_KERNEL_FIELD:
		raw[KERNEL_FIELD_OFFSET] = (uint8_t)(value);
		raw[KERNEL_FIELD_OFFSET + 1] = (uint8_t)(value >> 8);
		raw[KERNEL_FIELD_OFFSET + 2] = (uint8_t)(value >> 16);
		raw[KERNEL_FIELD_OFFSET + 3] = (uint8_t)(value >> 24);
		break;

	case VBNV_DEV_BOOT_USB:
		if (value)
			raw[DEV_FLAGS_OFFSET] |= DEV_BOOT_USB_MASK;
		else
			raw[DEV_FLAGS_OFFSET] &= ~DEV_BOOT_USB_MASK;
		break;

	case VBNV_DEV_BOOT_LEGACY:
		if (value)
			raw[DEV_FLAGS_OFFSET] |= DEV_BOOT_LEGACY_MASK;
		else
			raw[DEV_FLAGS_OFFSET] &= ~DEV_BOOT_LEGACY_MASK;
		break;

	case VBNV_DEV_BOOT_SIGNED_ONLY:
		if (value)
			raw[DEV_FLAGS_OFFSET] |= DEV_BOOT_SIGNED_ONLY_MASK;
		else
			raw[DEV_FLAGS_OFFSET] &= ~DEV_BOOT_SIGNED_ONLY_MASK;
		break;

	case VBNV_DISABLE_DEV_REQUEST:
		if (value)
			raw[BOOT_OFFSET] |= BOOT_DISABLE_DEV_REQUEST;
		else
			raw[BOOT_OFFSET] &= ~BOOT_DISABLE_DEV_REQUEST;
		break;

	case VBNV_OPROM_NEEDED:
		if (value)
			raw[BOOT_OFFSET] |= BOOT_OPROM_NEEDED;
		else
			raw[BOOT_OFFSET] &= ~BOOT_OPROM_NEEDED;
		break;

	case VBNV_CLEAR_TPM_OWNER_REQUEST:
		if (value)
			raw[TPM_FLAGS_OFFSET] |= TPM_CLEAR_OWNER_REQUEST;
		else
			raw[TPM_FLAGS_OFFSET] &= ~TPM_CLEAR_OWNER_REQUEST;
		break;

	case VBNV_CLEAR_TPM_OWNER_DONE:
		if (value)
			raw[TPM_FLAGS_OFFSET] |= TPM_CLEAR_OWNER_DONE;
		else
			raw[TPM_FLAGS_OFFSET] &= ~TPM_CLEAR_OWNER_DONE;
		break;

	case VBNV_BACKUP_NVRAM_REQUEST:
		if (value)
			raw[BOOT_OFFSET] |= BOOT_BACKUP_NVRAM;
		else
			raw[BOOT_OFFSET] &= ~BOOT_BACKUP_NVRAM;
		break;


	default:
		return 1;
	}

	/* Need to regenerate CRC, since the value changed. */
	context->regenerate_crc = 1;
	return 0;
}

/* These are the fields of the nvram that we want to back up. */
static const VbNvParam backup_params[] = {
	VBNV_KERNEL_FIELD,
	VBNV_LOCALIZATION_INDEX,
	VBNV_DEV_BOOT_USB,
	VBNV_DEV_BOOT_LEGACY,
	VBNV_DEV_BOOT_SIGNED_ONLY,
};

/* We can't back things up if there isn't enough storage. */
BUILD_ASSERT(VBNV_BLOCK_SIZE <= BACKUP_NV_SIZE);

int RestoreNvFromBackup(VbNvContext *vnc)
{
	VbNvContext bvnc;
	uint32_t value;
	int i;

	VBDEBUG(("TPM: %s()\n", __func__));

	if (TPM_SUCCESS != RollbackBackupRead(bvnc.raw))
		return 1;

	VbNvSetup(&bvnc);
	if (bvnc.regenerate_crc) {
		VBDEBUG(("TPM: Oops, backup is no good.\n"));
		return 1;
	}

	for (i = 0; i < ARRAY_SIZE(backup_params); i++) {
		VbNvGet(&bvnc, backup_params[i], &value);
		VbNvSet(vnc, backup_params[i], value);
	}

	/* VbNvTeardown(&bvnc); is not needed. We're done with it. */
	return 0;
}

int SaveNvToBackup(VbNvContext *vnc)
{
	VbNvContext bvnc;
	uint32_t value;
	int i;

	VBDEBUG(("TPM: %s()\n", __func__));

	/* Read it first. No point in writing the same data. */
	if (TPM_SUCCESS != RollbackBackupRead(bvnc.raw))
		return 1;

	VbNvSetup(&bvnc);
	VBDEBUG(("TPM: existing backup is %s\n",
		 bvnc.regenerate_crc ? "bad" : "good"));

	for (i = 0; i < ARRAY_SIZE(backup_params); i++) {
		VbNvGet(vnc, backup_params[i], &value);
		VbNvSet(&bvnc, backup_params[i], value);
	}

	VbNvTeardown(&bvnc);

	if (!bvnc.raw_changed) {
		VBDEBUG(("TPM: Nothing's changed, not writing backup\n"));
		/* Clear the request flag, since we're happy. */
		VbNvSet(vnc, VBNV_BACKUP_NVRAM_REQUEST, 0);
		return 0;
	}

	if (TPM_SUCCESS == RollbackBackupWrite(bvnc.raw)) {
		/* Clear the request flag if we wrote successfully too */
		VbNvSet(vnc, VBNV_BACKUP_NVRAM_REQUEST, 0);
		return 0;
	}

	VBDEBUG(("TPM: Sorry, couldn't write backup.\n"));
	return 1;
}
