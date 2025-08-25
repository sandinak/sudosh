int main() { 
    extern int sudo_compat_mode_flag; 
    sudo_compat_mode_flag = 0; 
    printf("Flag is: %d\n", sudo_compat_mode_flag); 
    return 0; 
}
