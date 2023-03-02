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

#define EFIVAR_SYSFS_NEW_VAR   "/sys/firmware/efi/vars/new_var"

int create_efi_var(const struct efi_variable *var)
{
    FILE* new_var = fopen(EFIVAR_SYSFS_NEW_VAR, "w");
    int error = 0;

    if (!new_var)
        return errno;
    if (fwrite(var, sizeof(*var), 1, new_var) != sizeof(*var))
        error = errno;    
    fclose(new_var);

    return error;
}

int create_efi_var_compat(const struct efi_variable_compat *var)
{
    FILE* new_var = fopen(EFIVAR_SYSFS_NEW_VAR, "w");
    int error = 0;

    if (!new_var)
        return errno;
    if (fwrite(var, sizeof(*var), 1, new_var) != sizeof(*var))
        error = errno;    
    fclose(new_var);

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

#define SHIM_BOOL_TRUE(var, Name) \
    struct efi_variable var = { \
	    .variable_name = Name, \
	    .vendor_guid = SHIM_LOCK_GUID, \
	    .data_size = 1, \
	    .data = { 1 }, \
    	.status = 0, \
	    .attributes = EFI_VARIABLE_NON_VOLATILE | \
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | \
                      EFI_VARIABLE_RUNTIME_ACCESS \
    }

#define SHIM_BOOL_TRUE_COMPAT(var, Name) \
    struct efi_variable_compat var = { \
	    .variable_name = Name, \
	    .vendor_guid = SHIM_LOCK_GUID, \
	    .data_size = 1, \
	    .data = { 1 }, \
    	.status = 0, \
	    .attributes = EFI_VARIABLE_NON_VOLATILE | \
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | \
                      EFI_VARIABLE_RUNTIME_ACCESS \
    }

#define GREETINGS \
    "This program sets NVRAM UEFI variables to configure the Linux boot shim.\n"\
    "NOTE: The device it is running on might be rendered broken!\n"\
    "Proceed at your own risk!\n"\
    "Create the variables? (enter y to accept, anything else to refuse): "

#define EXIT_NO_CHANGES \
    "Exiting, no changes have been made."

int main(int argc, char** argv)
{
    bool compat_mode = false;
    int error = 0;

    puts(GREETINGS);
    if (getchar() != 'y') {
        puts(EXIT_NO_CHANGES);
        return -2;
    }

    if (argc == 2 && !strcmp(argv[1], "-c")) {
        puts("using the compatibility mode");
        compat_mode = true;
    }

    if (compat_mode) {
        SHIM_BOOL_TRUE_COMPAT(compat_verbose, SHIM_FALLBACK_VERBOSE);
        SHIM_BOOL_TRUE_COMPAT(compat_no_reboot, SHIM_FB_NO_REBOOT);

        error = create_efi_var_compat(&compat_verbose);
        if (error)
            printf("creating 'fallback_verbose' NVRAM UEFI var failed, error %d\n", error);
        error = create_efi_var_compat(&compat_no_reboot);
        if (error)
            printf("creating 'fb_no_reboot' NVRAM UEFI var failed, error %d\n", error);        
    } else {
        SHIM_BOOL_TRUE(verbose, SHIM_FALLBACK_VERBOSE);
        SHIM_BOOL_TRUE(no_reboot, SHIM_FB_NO_REBOOT);

        error = create_efi_var(&verbose);
        if (error)
            printf("creating 'fallback_verbose' NVRAM UEFI var failed, error %d\n", error);
        error = create_efi_var(&no_reboot);
        if (error)
            printf("creating 'fb_no_reboot' NVRAM UEFI var failed, error %d\n", error);        
    }

    return 0;
}
