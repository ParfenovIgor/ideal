{
    def a int
    def b ptr
    b := a + 0
    // b := alloc (2)
    // b := a + 1
    if (1) {
        b := b + 100
    }
    else {
        // b := alloc (10)
    }
    while (2) {
        a := 2
        //def c ptr
        // c := alloc (2)
        // b := a + 0
        if (1) {
           // b := b + 1
        }
    }
    if (1) {
        b := a + 0
        b := alloc (3)
        //b := a + 3
        if (1) {
            //b := alloc (5)
        }
    }
    else {
        //b := alloc (4)
        b := a + 2
    }
}


