bool is_entry_boundary(){
    return global_tick!=0 && global_tick %128==0;
}

