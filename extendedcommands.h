void
show_choose_zip_menu();

int 
erase_root(const char *root);

int
do_nandroid_backup(const char* backup_name);

int
do_nandroid_restore();

void
show_nandroid_restore_menu();

void
show_nandroid_menu();

void
show_partition_menu();

void
show_choose_zip_menu();

int
install_zip(const char* packagefilepath);

int
__system(const char *command);

void
show_advanced_menu();

int
format_non_mtd_device(const char* root);

void
handle_failure();

int
confirm_selection(const char* title, const char* confirm);

void
show_install_update_menu();
