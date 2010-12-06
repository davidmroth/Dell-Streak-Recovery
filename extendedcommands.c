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
#include "recovery_ui.h"

#include "commands.h"
#include "amend/amend.h"
#include "mtdutils/dump_image.h"

//LOCAL_C_INCLUDES += bootable/recovery/mtdutils
#include "mtdutils.h"

//LOCAL_C_INCLUDES += bootable/recovery/miniui
#include "minui.h"

//LOCAL_C_INCLUDES += bootable/recovery/minzip
#include "DirUtil.h"


#include "extendedcommands.h"
#include "nandroid.h"

int signature_check_enabled = 0;
char verifcation_status[PATH_MAX];

int script_assert_enabled = 1;

//Non-volital Setting
static const char *CHECK_SIG_VERIFY = "CACHE:recovery/.enable_verify";
static const char *SDCARD_PACKAGE_FILE = "SDCARD:update.zip";

void toggle_setting(const char* setting)
{
    FILE *file;
    char path[PATH_MAX] = "";

    ensure_root_path_mounted(setting);
    if (translate_root_path(setting, path, sizeof(path)) == NULL) 
    {
       LOGE("Bad path %s (%s)\n", path, setting);
       exit(1);
    }

    signature_check_enabled = !signature_check_enabled;
    if (signature_check_enabled) 
    {
        file = fopen(path, "w");
        fclose(file);
        LOGI("Signature check enable (creating: %s).\n", path);
    } 
    else 
    {
	    if (remove(path)) LOGE("Error removing: %s\n", path);
        LOGI("Signature check disabled (removing: %s).\n", path);
    }

    //Change menu display
    sprintf(verifcation_status, "%s signature verification", signature_check_enabled ? "disable" : "enable");
}

int erase_root(const char *root) {
	ui_set_background(BACKGROUND_ICON_INSTALLING);
	ui_show_indeterminate_progress();
	ui_print("Formatting %s...\n", root);
	return format_root_device(root);
}

int format_non_mtd_device(const char* root)
{
    // if this is SDEXT:, don't worry about it.
    if (0 == strcmp(root, "SDEXT:"))
    {
        struct stat st;
        if (0 != stat(SDEXT_DEVICE, &st))
        {
            ui_print("No app2sd partition found. Skipping format of /sd-ext.\n");
            return 0;
        }
    }

    char path[PATH_MAX];
    translate_root_path(root, path, PATH_MAX);
    if (0 != ensure_root_path_mounted(root))
    {
        ui_print("Error mounting %s!\n", path);
        ui_print("Skipping format...\n");
        return 0;
    }

    static char tmp[PATH_MAX];
    sprintf(tmp, "rm -rf %s/* 2>&1 /dev/null", path);
    __system(tmp);
    sprintf(tmp, "rm -rf %s/.* 2>&1 /dev/null", path);
    __system(tmp);
    
    return 0;
}

int install_zip(const char* packagefilepath)
{
    ui_print("\n-- Installing: %s\n", packagefilepath);
#ifndef BOARD_HAS_NO_MISC_PARTITION
    set_sdcard_update_bootloader_message();
#endif
    int status = install_package(packagefilepath);
    ui_reset_progress();
    if (status != INSTALL_SUCCESS) {
        ui_set_background(BACKGROUND_ICON_ERROR);
        ui_print("Installation aborted.\n");
        return 1;
    } 
#ifndef BOARD_HAS_NO_MISC_PARTITION
    if (firmware_update_pending()) {
        ui_print("\nReboot via menu to complete\ninstallation.\n");
    }
#endif
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_print("\nInstall from sdcard complete.\n");
    return 0;
}

int is_setting_enabled(const char* filename)
{
    struct stat file_info;
    char path[PATH_MAX] = "";

    if (translate_root_path(filename, path, sizeof(path)) == NULL) {
        LOGE("Bad path %s\n", filename);
        exit(1);
    }

    if (0 == stat(path, &file_info))
        return 1;
    else
        return 0;
}

void show_install_update_menu()
{
    int chosen_item;
    static char* headers[] = {  "Apply update from .zip file on SD card",
                                "",
                                NULL 
    };

    signature_check_enabled = is_setting_enabled(CHECK_SIG_VERIFY);
    sprintf(verifcation_status, "%s signature verification", signature_check_enabled ? "disable" : "enable");

    char* list[] = {  "apply sdcard:update.zip",
                      "choose zip from sdcard",
                      verifcation_status,
                      NULL
    };

    for (;;)
    {
        chosen_item = get_menu_selection(headers, list, 0);

        switch (chosen_item)
        {
            case 0:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /sdcard/update.zip"))
                    install_zip(SDCARD_PACKAGE_FILE);
                break;
            }

            case 1:
                show_choose_zip_menu();
                break;

            case 2:
                toggle_setting(CHECK_SIG_VERIFY);
                break;

            default:
                return;
        }
    }
}

void free_string_array(char** array)
{
    if (array == NULL)
        return;
    char* cursor = array[0];
    int i = 0;
    while (cursor != NULL)
    {
        free(cursor);
        cursor = array[++i];
    }
    free(array);
}

char** gather_files(const char* directory, const char* fileExtensionOrDirectory, int* numFiles)
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files = NULL;
    int pass;
    *numFiles = 0;
    int dirLen = strlen(directory);

    dir = opendir(directory);
    if (dir == NULL) {
        ui_print("Couldn't open directory.\n");
        return NULL;
    }
  
    unsigned int extension_length = 0;
    if (fileExtensionOrDirectory != NULL)
        extension_length = strlen(fileExtensionOrDirectory);
  
    int isCounting = 1;

    i = 0;
    for (pass = 0; pass < 2; pass++) {
        while ((de = readdir(dir)) != NULL) {

            // skip hidden files
            if (de->d_name[0] == '.')
                continue;
            
            // NULL means that we are gathering directories, so skip this
            if (fileExtensionOrDirectory != NULL) {

                // make sure that we can have the desired extension (prevent seg fault)
                if (strlen(de->d_name) < extension_length)
                    continue;

                // compare the extension
                if (strcmp(de->d_name + strlen(de->d_name) - extension_length, fileExtensionOrDirectory) != 0)
                    continue;

            } else {

                struct stat info;
                char fullFileName[PATH_MAX];

                strcpy(fullFileName, directory);
                strcat(fullFileName, de->d_name);
                stat(fullFileName, &info);

                // make sure it is a directory
                if (!(S_ISDIR(info.st_mode)))
                    continue;
            }
            
            if (pass == 0) {
                total++;
                continue;
            }
            
            files[i] = (char*) malloc(dirLen + strlen(de->d_name) + 2);

            strcpy(files[i], directory);
            strcat(files[i], de->d_name);
            if (fileExtensionOrDirectory == NULL)
                strcat(files[i], "/");

            i++;
        }

        if (pass == 1)
            break;
        if (total == 0)
            break;

        rewinddir(dir);
        *numFiles = total;
        files = (char**) malloc((total+1)*sizeof(char*));
        files[total]=NULL;
    }

    if(closedir(dir) < 0) {
        LOGE("Failed to close directory.");
    }

    if (total==0) {
        return NULL;
    }

	// sort the result
	if (files != NULL) {
		for (i = 0; i < total; i++) {
			int curMax = -1;
			int j;
			for (j = 0; j < total - i; j++) {
				if (curMax == -1 || strcmp(files[curMax], files[j]) < 0)
					curMax = j;
			}
			char* temp = files[curMax];
			files[curMax] = files[total - i - 1];
			files[total - i - 1] = temp;
		}
	}

    return files;
}

// pass in NULL for fileExtensionOrDirectory and you will get a directory chooser
char* choose_file_menu(const char* directory, const char* fileExtensionOrDirectory, const char* headers[])
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int numFiles = 0;
    int numDirs = 0;
    int i;
    char* return_value = NULL;
    int dir_len = strlen(directory);
    char** files = gather_files(directory, fileExtensionOrDirectory, &numFiles);
    char** dirs = NULL;

    if (fileExtensionOrDirectory != NULL)
        dirs = gather_files(directory, NULL, &numDirs);

    int total = numDirs + numFiles;
    if (total == 0)
    {
        ui_print("No files found.\n");
    } 
    else 
    {
        char** list = (char**) malloc((total + 1) * sizeof(char*));
        list[total] = NULL;

        for (i = 0 ; i < numFiles; i++) 
        {
            list[i] = strdup(files[i] + dir_len);
        }

        for (i = 0 ; i < numDirs; i++) 
        {
            list[numFiles + i] = strdup(dirs[i] + dir_len);
        }

        for (;;) 
        {
            int chosen_item = get_menu_selection(headers, list, 0);
            if (chosen_item == GO_BACK)
                break;

            static char ret[PATH_MAX];
            if (chosen_item > (numFiles - 1)) {

                char* subret = choose_file_menu(dirs[chosen_item - numFiles], fileExtensionOrDirectory, headers);

                if (subret != NULL) {
                    strcpy(ret, subret);
                    return_value = ret;
                    break;
                }

                continue;
            } 

            strcpy(ret, files[chosen_item]);
            return_value = ret;
            break;
        }

        free_string_array(list);
    }

    free_string_array(files);
    free_string_array(dirs);
    return return_value;
}

void show_choose_zip_menu()
{
    char* path[PATH_MAX];
    sprintf(path, "%s/", SDCARD_PATH);

    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char* headers[] = {  "Choose a zip to apply",
                                "",
                                NULL 
    };
    
    char* file = choose_file_menu(path, ".zip", headers);
    if (file == NULL)
        return;

    char sdcard_package_file[1024];
    strcpy(sdcard_package_file, "SDCARD:");
    strcat(sdcard_package_file,  file + strlen(SDCARD_PATH) + 1);
    static char* confirm_install  = "Confirm install?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Install %s", basename(file));

    if (confirm_selection(confirm_install, confirm))
        install_zip(sdcard_package_file);
}

// This was pulled from bionic: The default system command always looks
// for shell in /system/bin/sh. This is bad.
#define _PATH_BSHELL "/sbin/sh"

extern char **environ;
int
__system(const char *command)
{
  pid_t pid;
    sig_t intsave, quitsave;
    sigset_t mask, omask;
    int pstat;
    char *argp[] = {"sh", "-c", NULL, NULL};

    if (!command)        /* just checking... */
        return(1);

    argp[2] = (char *)command;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &omask);
    switch (pid = vfork()) {
    case -1:            /* error */
        sigprocmask(SIG_SETMASK, &omask, NULL);
        return(-1);
    case 0:                /* child */
        sigprocmask(SIG_SETMASK, &omask, NULL);
        execve(_PATH_BSHELL, argp, environ);
    _exit(127);
    }

    intsave = (sig_t)  bsd_signal(SIGINT, SIG_IGN);
    quitsave = (sig_t) bsd_signal(SIGQUIT, SIG_IGN);
    pid = waitpid(pid, (int *)&pstat, 0);
    sigprocmask(SIG_SETMASK, &omask, NULL);
    (void)bsd_signal(SIGINT, intsave);
    (void)bsd_signal(SIGQUIT, quitsave);
    return (pid == -1 ? -1 : pstat);
}

void show_nandroid_restore_menu()
{
    char* path[PATH_MAX];
    sprintf(path, "%s/%s/", SDCARD_PATH, BACKUP_PATH);

    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }
    
    static char* headers[] = {  "Choose an image to restore",
                                "",
                                NULL 
    };

    char* file = choose_file_menu(path, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm full restore?", "Yes - This could take a while"))
        nandroid_restore(file, 1, 1, 1, 1, 1, 1);
}

void show_mount_usb_storage_menu()
{
    char command[PATH_MAX];
    sprintf(command, "echo %s > /sys/devices/platform/msm_hsusb/gadget/lun0/file", SDCARD_DEVICE_PRIMARY);
    __system(command);
    static char* headers[] = {  "USB Mass Storage device",
                                "Leaving this menu to unmount",
                                "your SD card from your PC.",
                                "",
                                NULL 
    };
    
    static char* list[] = { "Unmount", NULL };
    
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0);
        if (chosen_item == GO_BACK || chosen_item == 0)
            break;
    }
    
    __system("echo '' > /sys/devices/platform/msm_hsusb/gadget/lun0/file");
}

int confirm_selection(const char* title, const char* confirm)
{
    struct stat info;
    if (0 == stat("/sdcard/.no_confirm", &info))
        return 1;

    char* confirm_headers[]  = {  title, "  THIS CAN NOT BE UNDONE.", "", NULL };
    char* items[] = { "No",
                      "No",
                      "No",
                      "No",
                      "No",
                      "No",
                      "No",
                      confirm, //" Yes -- wipe partition",   // [7
                      "No",
                      "No",
                      "No",
                      NULL };

    int chosen_item = get_menu_selection(confirm_headers, items, 0);
    return chosen_item == 7;
}

void show_nandroid_advanced_restore_menu()
{    
    char* path[PATH_MAX];
    sprintf(path, "%s/%s/", SDCARD_PATH, BACKUP_PATH);

    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char* advancedheaders[] = {  "Choose an image to restore",
                                "",
                                "Choose an image to restore",
                                "first. The next menu will",
                                "you more options.",
                                "",
                                NULL
    };

    char* file = choose_file_menu(path, NULL, advancedheaders);
    if (file == NULL)
        return;

    static char* headers[] = {  "Nandroid Advanced Restore",
                                "",
                                NULL
    };

    static char* list[] = { "restore boot",
                            "restore system",
                            "restore data",
                            "restore cache",
                            "restore firstboot",
                            "restore .android_secure",
                            NULL
    };


    static char* confirm_restore  = "Confirm restore?";

    int chosen_item = get_menu_selection(headers, list, 0);
    switch (chosen_item)
    {
        case 0:
            if (confirm_selection(confirm_restore, "Yes - Restore boot"))
                nandroid_restore(file, 1, 0, 0, 0, 0, 0);
            break;
        case 1:
            if (confirm_selection(confirm_restore, "Yes - Restore system"))
                nandroid_restore(file, 0, 1, 0, 0, 0, 0);
            break;
        case 2:
            if (confirm_selection(confirm_restore, "Yes - Restore data"))
                nandroid_restore(file, 0, 0, 1, 0, 0, 0);
            break;
        case 3:
            if (confirm_selection(confirm_restore, "Yes - Restore cache"))
                nandroid_restore(file, 0, 0, 0, 1, 0, 0);
            break;
        case 4:
            if (confirm_selection(confirm_restore, "Yes - Restore firstboot"))
                nandroid_restore(file, 0, 0, 0, 0, 1, 0);
            break;
        case 5:
            if (confirm_selection(confirm_restore, "Yes - Restore .android_secure"))
                nandroid_restore(file, 0, 0, 0, 0, 0, 1);
            break;
    }
}

void show_nandroid_menu()
{
    static char* headers[] = {  "Nandroid",
                                "",
                                NULL 
    };

    static char* list[] = { "full backup", 
                            "full restore",
                            "selective restore",
                            NULL
    };

	for(;;) 
	{ 
	    int chosen_item = get_menu_selection(headers, list, 0);

		if (chosen_item == GO_BACK)
			break;

    	switch (chosen_item)
	    {
    	    case 0:
        	    {
            		if (confirm_selection("Confirm full backup?", "Yes - This could take a while"))
					{
    	            	time_t t = time(NULL);
	    	            struct tm *tmp = localtime(&t);
	            	    char backup_path[PATH_MAX];
	            	    char time_path[PATH_MAX];

    	    	        if (tmp == NULL)
        	    		{
            	    		struct timeval tp;
	                	    gettimeofday(&tp, NULL);
    	                	sprintf(backup_path, "%s/%s/%d", SDCARD_PATH, BACKUP_PATH, (int)tp.tv_sec);

			            } else {
            		        strftime(time_path, PATH_MAX, "%F.%H.%M.%S", tmp);
                            sprintf(backup_path, "%s/%s/%s", SDCARD_PATH, BACKUP_PATH, time_path);
        	    	    }

	                	nandroid_backup(backup_path);
					}
    	        }
        	    break;

	        case 1:
    	        show_nandroid_restore_menu();
        	    break;

	        case 2:
    	        show_nandroid_advanced_restore_menu();
        	    break;
    	}
	}
}

void wipe_battery_stats()
{
    ensure_root_path_mounted("DATA:");
	remove("/data/system/batterystats.bin");
	ui_print("Battery stats wiped.\n");
	ensure_root_path_unmounted("DATA:");
}

void show_advanced_menu()
{
    static char* headers[] = {  "Advanced Menu",
                                "",
                                NULL
    };

    static char* list[] = { "nandroid",
                            "wipe dalvik cache",
			                "wipe battery stats",
                            "fix permissions",
                            "mount sdcard via PC",
                            "report error",
                            //"key test",
                            NULL
    };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
               show_nandroid_menu();
               break;
            case 1:
            {
                if (confirm_selection( "Confirm wipe?", "Yes - Wipe Dalvik Cache")) {
                    if (0 != ensure_root_path_mounted("DATA:"))
                        break;

                    __system("rm -r /data/dalvik-cache 2>&1 /dev/null");
                }
                ensure_root_path_unmounted("DATA:");
                ui_print("Dalvik Cache wiped.\n");
                break;
            }
            case 2:
                wipe_battery_stats();
                break;
            case 3:
            {
                ensure_root_path_mounted("SYSTEM:");
                ensure_root_path_mounted("DATA:");
                ensure_root_path_mounted("USERDATA:");
                ui_show_indeterminate_progress();
                ui_print("Fixing permissions...\n");
                __system("fix_permissions");
                ui_reset_progress();
                ui_print("Done!\n");
                break;
            }
            case 4:
        		show_mount_usb_storage_menu();
                break;
            case 5:
                handle_failure();
                break;
            case 6:
            {
                ui_print("Outputting key codes.\n");
                ui_print("Go back to end debugging.\n");
                int key;
                int action;
                do
                {
                    key = ui_wait_key();
                    action = device_handle_key(key, 1);
                    ui_print("Key: %d\n", key);
                }
                while (action != GO_BACK);
                break;
            }
        }
    }
}

void handle_failure()
{
    ensure_root_path_mounted("SDCARD:");
    mkdir("/sdcard/recovery", S_IRWXU);
    __system("cp /tmp/recovery.log /sdcard/recovery.log");
    ui_print("The recovery.log was copied to your /sdcard/.\n");
}
