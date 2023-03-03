{
    func foo (a ptr 2) {
        def b int
        def c int
        if (1) {
            b := $a
            a := a + 1
            c := $a
            a <- b
            a := a + -1
            a <- c
        }
    }
    
    def a ptr
    a := alloc (10)
    def b ptr
    def c int
    while (1) {
        // ...
        b := c + 0
        assume (b < a + 9)
        call foo (b)
    }
}

