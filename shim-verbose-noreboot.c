#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

struct efi_guid {
	uint32_t    data1;
	uint16_t    data2;
	uint16_t    data3;
	uint8_t     data4[8];
} __attribute__((aligned(4)));

#define EFI_VAR_NAME_LEN	1024
#define EFI_VAR_DATA_LEN	1024

struct efi_variable {
	uint16_t            variable_name[EFI_VAR_NAME_LEN/sizeof(uint16_t)];
	struct efi_guid     vendor_guid;
	uint64_t            data_size;
	uint8_t             data[EFI_VAR_DATA_LEN];
	uint64_t            status;
	uint32_t            attributes;
} __attribute__((packed));

struct efi_variable_compat {
	uint16_t            variable_name[EFI_VAR_NAME_LEN/sizeof(uint16_t)];
	struct efi_guid     vendor_guid;
	uint32_t            data_size;
	uint8_t             data[EFI_VAR_DATA_LEN];
	uint32_t            status;
	uint32_t            attributes;
} __attribute__((packed));

#define EFI_VARIABLE_NON_VOLATILE                           0x0000000000000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                     0x0000000000000002
#define EFI_VARIABLE_RUNTIME_ACCESS                         0x0000000000000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                  0x0000000000000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS             0x0000000000000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS  0x0000000000000020
#define EFI_VARIABLE_APPEND_WRITE	                        0x0000000000000040

#define EFI_NEW_VAR   "/sys/firmware/efi/vars/new_var"
#define EFI_DEL_VAR   "/sys/firmware/efi/vars/del_var"

int do_efi_var_op(const struct efi_variable *var, const char* op_path)
{
	if (!op_path || op_path[0] == '\x00')
		return 0;

	FILE* var_op_path = fopen(op_path, "w");
	int error = 0;

	if (!var_op_path)
		return errno;
	if (fwrite(var, sizeof(*var), 1, var_op_path) != sizeof(*var))
		error = errno;
	fclose(var_op_path);

	return error;
}

int do_efi_var_compat_op(const struct efi_variable_compat *var, const char* op_path)
{
	if (!op_path || op_path[0] == '\x00')
		return 0;

	FILE* var_op_path = fopen(op_path, "w");
	int error = 0;

	if (!var_op_path)
		return errno;
	if (fwrite(var, sizeof(*var), 1, var_op_path) != sizeof(*var))
		error = errno;
	fclose(var_op_path);

	return error;
}

#define SHIM_FALLBACK_VERBOSE        { 'F','A','L','L','B','A','C','K','_','V','E','R','B','O','S','E','\x0' }
#define SHIM_FB_NO_REBOOT            { 'F','B','_','N','O','_','R','E','B','O','O','T','\x0' }
#define SHIM_LOCK_GUID              { \
										0x605dab50, \
										0xe046, \
										0x4300, \
										{ 0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23 } \
									}

#define SHIM_BOOL(var, Name, ValueBool) \
	struct efi_variable var = { \
		.variable_name = Name, \
		.vendor_guid = SHIM_LOCK_GUID, \
		.data_size = 1, \
		.data = { (ValueBool) }, \
		.status = 0, \
		.attributes = EFI_VARIABLE_NON_VOLATILE | \
					  EFI_VARIABLE_BOOTSERVICE_ACCESS | \
					  EFI_VARIABLE_RUNTIME_ACCESS \
	}

#define SHIM_BOOL_COMPAT(var, Name, ValueBool) \
	struct efi_variable_compat var = { \
		.variable_name = Name, \
		.vendor_guid = SHIM_LOCK_GUID, \
		.data_size = 1, \
		.data = { (ValueBool) }, \
		.status = 0, \
		.attributes = EFI_VARIABLE_NON_VOLATILE | \
					  EFI_VARIABLE_BOOTSERVICE_ACCESS | \
					  EFI_VARIABLE_RUNTIME_ACCESS \
	}

enum shim_efi_var_op {
	SET,
	CLEAR,
	DELETE,
	DONT_CARE
};

const char* shim_efi_var_op_str(enum shim_efi_var_op op)
{
	switch (op)
	{
		case SET:
			return "Setting";
		case CLEAR:
			return "Clearing";
		case DELETE:
			return "Deleting";
		case DONT_CARE:
			return "Leaving intact";
	}

	assert(!"Unsupported operation");
}

const char* shim_efi_var_op_path(enum shim_efi_var_op op)
{
	switch (op)
	{
		case SET:
			return EFI_NEW_VAR;
		case CLEAR:
			return EFI_NEW_VAR;
		case DELETE:
			return EFI_DEL_VAR;
		case DONT_CARE:
			return "";
	}

	assert(!"Unsupported operation");
}

#define GREETINGS \
	"This program sets NVRAM UEFI variables to configure the Linux boot shim.\n"\
	"NOTE: The device it is running on might be rendered broken!\n"\
	"Proceed at your own risk!\n"\
	"Do you want to update the variables? (enter y to accept, anything else to refuse): "

#define EXIT_NO_CHANGES \
	"Exiting, no changes have been made."

int main(int argc, char** argv)
{
	enum shim_efi_var_op no_reboot = DONT_CARE;
	enum shim_efi_var_op verbose = DONT_CARE;

	int error = 0;

	puts(GREETINGS);
	if (getchar() != 'y') {
		puts(EXIT_NO_CHANGES);
		return -2;
	}

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[1], "noreboot=1")) {
			no_reboot = SET;
		} else if (!strcmp(argv[1], "noreboot=0")) {
			no_reboot = CLEAR;
		} else if (!strcmp(argv[1], "noreboot=")) {
			no_reboot = DELETE;
		} else if (!strcmp(argv[1], "verbose=1")) {
			verbose = SET;
		} else if (!strcmp(argv[1], "verbose=0")) {
			verbose = CLEAR;
		} else if (!strcmp(argv[1], "verbose=")) {
			verbose = DELETE;
		} else {
			assert(!"Unknown command line option");
		}
	}

	{
		SHIM_BOOL(efi_var, SHIM_FB_NO_REBOOT, no_reboot == SET);
		const char* op_str = shim_efi_var_op_str(no_reboot);
		const char* op_path = shim_efi_var_op_path(no_reboot);

		error = do_efi_var_op(&efi_var, op_path);

		printf("%s on 'FB_NO_REBOOT' NVRAM UEFI\n", op_str);
		if (error)
			printf("%s 'FB_NO_REBOOT' NVRAM UEFI var failed, error %d\n",
				   op_str,
				   error);
	}

	{
		SHIM_BOOL(efi_var, SHIM_FALLBACK_VERBOSE, verbose == SET);
		const char* op_str = shim_efi_var_op_str(verbose);
		const char* op_path = shim_efi_var_op_path(verbose);

		error = do_efi_var_op(&efi_var, op_path);

		printf("%s on 'FALLBACK_VERBOSE' NVRAM UEFI\n", op_str);
		if (error)
			printf("%s 'FALLBACK_VERBOSE' NVRAM UEFI var failed, error %d\n",
				   op_str,
				   error);
	}

	return 0;
}
