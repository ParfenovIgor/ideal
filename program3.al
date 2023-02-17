{
    def arr ptr
    arr := alloc (50)
    def arr_ptr ptr
    arr_ptr := arr + 0
    def num int
    num := 2
    def i int
    def flag int
    while (num < 100) {
        i := 2
        flag := 0
        while (i < num) {
            if (1) {
                flag := true
            }
        }
        if (flag) {
            arr_ptr <- num
            arr_ptr := arr_ptr + 1
            assume (arr_ptr < arr + 50)
        }
        num := num + 1
    }
}


