{
    func queue_push (q ptr 50, sz ptr 1, v int) {
        def a ptr
        def b int
        a := q + 0
        b := $sz
        while (1) {
            a := a + 1
            assume (a < q + 50)
        }
        a <- v
        sz <- $sz + 1
    }

    func queue_pop (q ptr 50, sz ptr 1, v ptr 1) {
        def a ptr
        a := q + 0
        while (1) {
            a := a + 1
            assume (a < q + 50)
        }
        v <- $a
        sz <- $sz + -1
    }

    def queue ptr
    queue := alloc (50)
    def sz ptr
    sz := alloc (1)
    sz <- 0
    def v int
    v := 3
    call queue_push (queue, sz, v)
    v := 4
    call queue_push (queue, sz, v)
    def x ptr
    x := alloc (1)
    call queue_pop (queue, sz, x)
    call queue_pop (queue, sz, x)
}

