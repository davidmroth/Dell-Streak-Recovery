#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>


#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"

#include "roots.h"
#include "bootloader.h"
#include "recovery_ui.h"

#include "commands.h"
#include "amend/amend.h"

#include "mtdutils/dump_image.h"
#include "yaffs2/mkyaffs2image.h"
#include "yaffs2/unyaffs.h"

//LOCAL_C_INCLUDES += bootable/recovery/minui
#include "minui.h"

//LOCAL_C_INCLUDES += bootable/recovery/minzip
#include "DirUtil.h"

#include <sys/vfs.h>

#include "extendedcommands.h"
#include "nandroid.h"

int print_and_error(char* message) {
    ui_print(message);
    return 1;
}

int yaffs_files_total = 0;
int yaffs_files_count = 0;


void yaffs_callback(char* filename)
{
    char* justfile = basename(filename);
    if (strlen(justfile) < 30)
        ui_print(justfile);
    yaffs_files_count++;
    if (yaffs_files_total != 0)
        ui_set_progress((float)yaffs_files_count / (float)yaffs_files_total);
    ui_reset_text_col();
}

void compute_directory_stats(char* directory)
{
    char tmp[PATH_MAX];
    sprintf(tmp, "find %s -print -path %slost+found -prune | wc -l > /tmp/dircount", directory, directory);
    __system(tmp);
    char count_text[100];
    FILE* f = fopen("/tmp/dircount", "r");
    fread(count_text, 1, sizeof(count_text), f);
    fclose(f);
    yaffs_files_count = 0;
    yaffs_files_total = atoi(count_text);
    ui_reset_progress();
    ui_show_progress(1, 0);
}

int nandroid_backup_partition_extended(const char* backup_path, char* root, int umount_when_finished) {
    int ret = 0;
    char tmp[PATH_MAX];
    char mount_point[PATH_MAX];

    translate_root_path(root, mount_point, PATH_MAX);
    char* name = basename(mount_point);

    struct stat file_info;
    mkyaffs2image_callback callback = NULL;

    sprintf(tmp, "%s/%s/.hidenandroidprogress", SDCARD_PATH, BACKUP_PATH);
    if (0 != stat(tmp, &file_info)) {
        callback = yaffs_callback;
    }
    
    ui_print("Backing up %s...\n", name);
    if (0 != (ret = ensure_root_path_mounted(root) != 0)) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }

    compute_directory_stats(mount_point);

    sprintf(tmp, "%s/%s.img", backup_path, name);
    ret = mkyaffs2image(mount_point, tmp, 0, callback);
    if (umount_when_finished) {
        ensure_root_path_unmounted(root);
    }
    if (0 != ret) {
        ui_print("Error while making a yaffs2 image of %s!\n", mount_point);
        return ret;
    }
    return 0;
}

int nandroid_backup_partition(const char* backup_path, char* root) {
    return nandroid_backup_partition_extended(backup_path, root, 1);
}

int nandroid_backup(const char* backup_path)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    
    if (ensure_root_path_mounted("SDCARD:") != 0)
        return print_and_error("Can't mount /sdcard\n");
    
    int ret;
    struct statfs s;

    if (0 != (ret = statfs(SDCARD_PATH, &s)))
        return print_and_error("Unable to stat /sdcard\n");

    uint64_t bavail = s.f_bavail;
    uint64_t bsize = s.f_bsize;
    uint64_t sdcard_free = bavail * bsize;
    uint64_t sdcard_free_mb = sdcard_free / (uint64_t)(1024 * 1024);
    ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);

    if (sdcard_free_mb < 150)
        ui_print("There may not be enough free space to complete backup... continuing...\n");
    
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s", backup_path);
    __system(tmp);

    ui_print("Backing up boot...\n");
    sprintf(tmp, "%s/%s", backup_path, "boot.img");
    ret = dump_image("boot", tmp, NULL);
    if (0 != ret)
        return print_and_error("Error while dumping boot image!\n");

    ui_print("Backing up recovery...\n");
    sprintf(tmp, "%s/%s", backup_path, "recovery.img");
    ret = dump_image("recovery", tmp, NULL);
    if (0 != ret)
        return print_and_error("Error while dumping recovery image!\n");

    if (0 != (ret = nandroid_backup_partition(backup_path, "SYSTEM:")))
        return ret;

    if (0 != (ret = nandroid_backup_partition(backup_path, "DATA:")))
        return ret;

    if (0 != (ret = nandroid_backup_partition_extended(backup_path, "CACHE:", 0)))
        return ret;

    if (0 != (ret = nandroid_backup_partition_extended(backup_path, "USERDATA:", 0)))
        return ret;

    sprintf(tmp, "%s/.android_secure", SDCARD_PATH);
    struct stat st;
    if (0 != stat(tmp, &st))
    {
        ui_print("No /sdcard/.android_secure found. Skipping backup of applications on external storage.\n");
    }
    else
    {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, "ANDROID_SECURE:", 0)))
            return ret;
    }

    ui_print("Generating md5 sum...\n");
    sprintf(tmp, "nandroid-md5.sh %s", backup_path);
    if (0 != (ret = __system(tmp))) {
        ui_print("Error while generating md5 sum!\n");
        return ret;
    }
    
    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nBackup complete!\n");
    return 0;
}

typedef int (*format_function)(char* root);

static void ensure_directory(const char* dir) {
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s", dir);
    __system(tmp);
}

int nandroid_restore_partition_extended(const char* backup_path, const char* root, int umount_when_finished) {
    int ret = 0;
    char mount_point[PATH_MAX];
    translate_root_path(root, mount_point, PATH_MAX);
    char* name = basename(mount_point);
    
    char tmp[PATH_MAX];
    sprintf(tmp, "%s/%s.img", backup_path, name);
    struct stat file_info;
    if (0 != (ret = stat(tmp, &file_info))) {
        ui_print("%s.img not found. Skipping restore of %s.\n", name, mount_point);
        return 0;
    }

    ensure_directory(mount_point);

    sprintf(tmp, "%s/%s/.hidenandroidprogress", SDCARD_PATH, BACKUP_PATH);
    unyaffs_callback callback = NULL;
    if (0 != stat(tmp, &file_info)) {
        callback = yaffs_callback;
    }

    ui_print("Restoring %s...\n", name);

    if (0 != (ret = ensure_root_path_unmounted(root))) {
        ui_print("Can't unmount %s!\n", mount_point);
        return ret;
    }

    if (0 != (ret = format_root_device(root))) {
        ui_print("Error while formatting %s!\n", root);
        return ret;
    }
    
    if (0 != (ret = ensure_root_path_mounted(root))) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }
    
    sprintf(tmp, "%s/%s.img", backup_path, name);
    if (0 != (ret = unyaffs(tmp, mount_point, callback))) {
        ui_print("Error while restoring %s!\n", mount_point);
        return ret;
    }

    if (umount_when_finished) {
        ensure_root_path_unmounted(root);
    }
    
    return 0;
}

int nandroid_restore_partition(const char* backup_path, const char* root) {
    return nandroid_restore_partition_extended(backup_path, root, 1);
}

int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_userdata, int restore_android_secure)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    yaffs_files_total = 0;

    if (ensure_root_path_mounted("SDCARD:") != 0)
        return print_and_error("Can't mount /sdcard\n");
    
    char tmp[PATH_MAX];

    ui_print("Checking MD5 sums...\n");
    sprintf(tmp, "cd %s && md5sum -c nandroid.md5", backup_path);
    if (0 != __system(tmp))
        return print_and_error("MD5 mismatch!\n");
    
    int ret;
    if (restore_boot)
    {
        ui_print("Erasing boot before restore...\n");
        if (0 != (ret = format_root_device("BOOT:")))
            return print_and_error("Error while formatting BOOT:!\n");
        sprintf(tmp, "flash_image boot %s/boot.img", backup_path);
        ui_print("Restoring boot image...\n");
        if (0 != (ret = __system(tmp))) {
            ui_print("Error while flashing boot image!");
            return ret;
        }
    }
    
    if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "SYSTEM:")))
        return ret;

    if (restore_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "CACHE:", 0)))
        return ret;

    if (restore_userdata && 0 != (ret = nandroid_restore_partition(backup_path, "USERDATA:")))
        return ret;

    if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "DATA:")))
    {
        return ret;
    } 
    else 
    {
        ui_print("Data restore complete. Checking and repairing permissions...\n");
        __system("fix_permissions");
    }

    if (restore_android_secure && 0 != (ret = nandroid_restore_partition_extended(backup_path, "ANDROID_SECURE:", 0))) 
       return ret;

        

    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nRestore complete!\n");
    return 0;
}

void nandroid_generate_timestamp_path(char* backup_path)
{
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    char path[PATH_MAX];
    char time_path[PATH_MAX];

    if (tmp == NULL)
    {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(backup_path, "%s/%s/%d", SDCARD_PATH, BACKUP_PATH, (int)tp.tv_sec);
    }
    else
    {
        sprintf(path, "%s/%s", SDCARD_PATH, BACKUP_PATH);
        strftime(time_path, PATH_MAX, "%F.%H.%M.%S", tmp);
        sprintf(backup_path, "%s/%s", path, time_path);
    }
}

int nandroid_usage()
{
    printf("Usage: nandroid backup\n");
    printf("Usage: nandroid restore <directory>\n");
    return 1;
}

int nandroid_main(int argc, char** argv)
{
    if (argc > 3 || argc < 2)
        return nandroid_usage();
    
    if (strcmp("backup", argv[1]) == 0)
    {
        if (argc != 2)
            return nandroid_usage();
        
        char backup_path[PATH_MAX];
        nandroid_generate_timestamp_path(backup_path);
        return nandroid_backup(backup_path);
    }

    if (strcmp("restore", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_restore(argv[2], 1, 1, 1, 1, 1, 1);
    }
    
    return nandroid_usage();
}
