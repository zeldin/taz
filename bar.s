
foo	.set	8				


	.ifge	foo,4

bar	.set	2

	.endc

	.iflt	foo,4
	
bar	.set	7
	
	.endc	
		
	add	d0,d0
	add	d1,a3
	addq.b	#foo,(4,a1,d2.w)

;	.endc
		
foo	.set	4
