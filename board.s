

		incdir	"../AmigaInclude"

		include	"exec/types.i"
		include	"exec/nodes.i"
		include	"exec/resident.i"
		include	"exec/initializers.i"
		include	"exec/devices.i"
		include	"exec/io.i"
		include	"exec/errors.i"
		include	"libraries/configvars.i"
		include	"libraries/expansionbase.i"
		include	"devices/trackdisk.i"


MANUF_ID	EQU	2011
PRODUCT_ID	EQU	17
SERIAL_NO	EQU	1

SIZE_FLAG	EQU	1


VERSION		EQU	1
REVISION	EQU	1


_LVOFindResident	EQU	-$60
_LVODisable		EQU	-$78
_LVOEnable		EQU	-$7E
_LVOEnqueue		EQU	-$10E
_LVOReplyMsg		EQU	-$17A
_LVOOpenLibrary		EQU	-$228
_LVOCloseLibrary	EQU	-$19E

_LVOGetCurrentBinding	EQU	-$8A


		STRUCTURE mydev,DD_SIZE
		APTR	md_Board
		APTR	md_ConfigDev
		APTR	md_ExecBase
		ULONG	md_NumUnits
		UBYTE	md_Flags
		UBYTE	md_Pad0
		UWORD	md_Pad1
		LABEL	mydev_SIZEOF

board_start

exp_rom
		dc.w	$d000	; Z2, not memory, we have a ROM driver
		dc.w	(SIZE_FLAG<<12)&$7000

		dc.w	(~(PRODUCT_ID<<8))&$f000
		dc.w	(~(PRODUCT_ID<<12))&$f000

		dc.w	$0000,$0000,$0000,$0000

		dc.w	(~MANUF_ID)&$f000
		dc.w	(~(MANUF_ID<<4))&$f000
		dc.w	(~(MANUF_ID<<8))&$f000
		dc.w	(~(MANUF_ID<<12))&$f000

		dc.w	(~(SERIAL_NO>>16))&$f000
		dc.w	(~(SERIAL_NO>>12))&$f000
		dc.w	(~(SERIAL_NO>>8))&$f000
		dc.w	(~(SERIAL_NO>>4))&$f000
		dc.w	(~SERIAL_NO)&$f000
		dc.w	(~(SERIAL_NO<<4))&$f000
		dc.w	(~(SERIAL_NO<<8))&$f000
		dc.w	(~(SERIAL_NO<<12))&$f000

		dc.w	(~DIAG)&$f000
		dc.w	(~(DIAG<<4))&$f000
		dc.w	(~(DIAG<<8))&$f000
		dc.w	(~(DIAG<<12))&$f000

		dc.w	$0000,$0000,$0000,$0000
		dc.w	$0000,$0000,$0000,$0000


exp_ctrl
		dcb.w	32,0


board_unit	dc.w	0
board_cntrl	dc.w	0
board_addr	dc.l	0
board_offs	dc.l	0
board_len	dc.l	0


diag_area
		dc.b	DAC_WORDWIDE!DAC_CONFIGTIME	; da_Config
		dc.b	0				; da_Flags
		dc.w	end_copy-diag_area		; da_Size
		dc.w	diag_entry-diag_area		; da_DiagPoint
		dc.w	boot_entry-diag_area		; da_BootPoint
		dc.w	dev_name-diag_area		; da_Name
		dc.w	0				; da_Reserved01
		dc.w	0				; da_Reserved02
		
DIAG		EQU	(diag_area-board_start)

romtag
		dc.w	RTC_MATCHWORD		; RT_MATCHWORD
rt_match	dc.l	romtag-diag_area	; RT_MATCHTAG
rt_end		dc.l	end_copy-diag_area	; RT_ENDSKIP
		dc.b	RTF_COLDSTART!RTF_AUTOINIT	; RT_FLAGS
		dc.b	VERSION			; RT_VERSION
		dc.b	NT_DEVICE		; RT_TYPE
		dc.b	20			; RT_PRI
rt_name		dc.l	dev_name-diag_area	; RT_NAME
rt_id		dc.l	id_string-diag_area	; RT_IDSTRING
rt_init		dc.l	init-diag_area		; RT_INIT

init
		dc.l	mydev_SIZEOF
i_functab	dc.l	func_table-board_start
i_datatab	dc.l	data_table-board_start
i_init		dc.l	dev_init-board_start

boot_node
		dc.l	0,0
		dc.b	NT_BOOTNODE,0
		dc.l	0
		dc.w	0
		dc.l	0


dev_name	dc.b	'hardfile.device',0
id_string	dc.b	'hardfile ',48+VERSION,'.',48+REVISION,13,10,0

dos_name	dc.b	'dos.library',0
explibname	dc.b	'expansion.library',0

		cnop	0,2


diag_entry	; success = DiagEntry(BoardBase, DiagCopy, configDev)
		; d0                  a0         a2        a3

		lea	patch_table-board_start(a0),a1
		move.l	a2,d1
dloop		move.w	(a1)+,d0
		bmi.s	bpatches
		add.l	d1,0(a2,d0.w)
		bra.s	dloop
bpatches	move.l	a0,d1
bloop		move.w	(a1)+,d0
		bmi.s	endpatches
		add.l	d1,0(a2,d0.w)
		bra.s	dloop
endpatches	moveq	#1,d0
		rts

boot_entry
		lea	explibname(pc),a1
		moveq	#37,d0
		jsr	_LVOOpenLibrary(a6)
		tst.l	d0
		beq.s	no_v37
		move.l	d0,a1
		bset.b	#EBB_SILENTSTART,eb_Flags(a1)
		jsr	_LVOCloseLibrary(a6)
no_v37		lea	dos_name(pc),a1
		jsr	_LVOFindResident(a6)
		tst.l	d0
		beq.s	no_dos
		move.l	d0,a0
		move.l	RT_INIT(a0),a0
		moveq	#0,d0
		rts
no_dos		moveq	#-1,d0
		rts

end_copy


patch_table
	; pointers that need to be patched to point to the diag copy
		dc.w	rt_match-diag_area
		dc.w	rt_end-diag_area
		dc.w	rt_name-diag_area
		dc.w	rt_id-diag_area
		dc.w	rt_init-diag_area
		dc.w	-1

	; pointers that need to be patched to point to the board
		dc.w	i_functab-diag_area
		dc.w	i_datatab-diag_area
		dc.w	i_init-diag_area
		dc.w	-1


data_table
		INITBYTE LN_TYPE,NT_DEVICE
		INITBYTE LIB_FLAGS,LIBF_SUMUSED!LIBF_CHANGED
		INITWORD LIB_VERSION,VERSION
		INITWORD LIB_REVISION,REVISION
		dc.l	0

func_table
		dc.w	-1
		dc.w	dev_open-func_table
		dc.w	dev_close-func_table
		dc.w	dev_expunge-func_table
		dc.w	dev_null-func_table
		dc.w	dev_beginio-func_table
		dc.w	dev_abortio-func_table
		dc.w	-1

dev_init
		movem.l	d1-d7/a0-a6,-(sp)
		move.l	d0,a5
		move.l	a6,md_ExecBase(a5)
		lea	explibname(pc),a1
		moveq	#0,d0
		jsr	_LVOOpenLibrary(a6)
		tst.l	d0
		beq.s	init_error
		move.l	d0,a4
		moveq	#0,d3
		lea	md_ConfigDev(a5),a0
		moveq	#4,d0
		exg	a4,a6
		jsr	_LVOGetCurrentBinding(a6)
		exg	a4,a6
		move.l	md_ConfigDev(a5),d0
		beq.s	init_end
		move.l	d0,a0
		move.l	cd_BoardAddr(a0),md_Board(a5)
		bclr.b	#CDB_CONFIGME,cd_Flags(a0)
		move.l	md_Board(a5),a1
		move.l	board_len-board_start(a1),md_NumUnits(a5)
		move.l	cd_Rom+er_Reserved0c(a0),a1
		lea	dev_name-diag_area(a1),a1
		move.l	a1,LN_NAME(a5)
		lea	id_string-dev_name(a1),a1
		move.l	a1,LIB_IDSTRING(a5)
		move.l	a5,cd_Driver(a0)

		lea	boot_node-id_string(a1),a1
		move.l	a0,bn_DeviceNode(a1)
		move.l	LN_NAME(a5),LN_NAME(a1)
		lea	eb_MountList(a4),a0
		jsr	_LVOEnqueue(a6)

		;; 

init_end
		move.l	a4,a1
		jsr	_LVOCloseLibrary(a6)
		move.l	a5,d0
init_error
		movem.l	(sp)+,d1-d7/a0-a6
		rts



dev_open
		move.l	a2,-(sp)
		move.l	a1,a2
		cmp.l	md_NumUnits(a6),d0
		bhs.s	open_range_error
		move.l	d0,IO_UNIT(a2)
		addq.w	#1,LIB_OPENCNT(a6)
		bclr.b	#LIBB_DELEXP,md_Flags(a6)
		moveq	#0,d0
		clr.b	IO_ERROR(a2)
open_end
		move.l	(sp)+,a2
		rts

open_range_error
		moveq	#IOERR_OPENFAIL,d0
		move.b	d0,IO_ERROR(a2)
		bra.s	open_end

dev_close
		move.l	a2,-(sp)
		move.l	a1,a2
		moveq	#-1,d0
		move.l	d0,IO_UNIT(a2)
		move.l	d0,IO_DEVICE(a2)
		moveq	#0,d0
		subq.w	#1,LIB_OPENCNT(a6)
		bne.s	close_end
		btst.b	#LIBB_DELEXP,md_Flags(a6)
		beq.s	close_end
		bsr.s	dev_expunge
close_end	move.l	(sp)+,a2
		rts

dev_expunge
		; No! We don't want any expunges here!
		moveq	#0,d0
		rts

dev_null
		moveq	#0,d0
		rts

dev_abortio
		; Abort?  Yeah, right...
		moveq	#IOERR_NOCMD,d0
		rts


dev_beginio
		move.b	#NT_MESSAGE,LN_TYPE(a1)
		moveq	#0,d0
		move.b	d0,IO_ERROR(a1)
		move.l	d0,IO_ACTUAL(a1)
		move.w	IO_COMMAND(a1),d0
		cmp.w	#MAX_COMMAND,d0
		bhs.s	beginio_nocmd
		asl.w	#1,d0
		move.w	cmdtable(pc,d0.w),d0
		jsr	cmdtable(pc,d0.w)
		btst.b	#IOB_QUICK,IO_FLAGS(a1)
		bne.s	termio_end
		move.l	a6,-(sp)
		move.l	md_ExecBase(a6),a6
		jsr	_LVOReplyMsg(a6)
		move.l	(sp)+,a6
termio_end
		rts

beginio_nocmd
dev_rawread
dev_rawwrite
dev_invalid
		move.b	#IOERR_NOCMD,IO_ERROR(a1)
		rts

cmdtable	dc.w	dev_invalid-cmdtable
		dc.w	dev_reset-cmdtable
		dc.w	dev_readwrite-cmdtable
		dc.w	dev_readwrite-cmdtable
		dc.w	dev_update-cmdtable
		dc.w	dev_clear-cmdtable
		dc.w	dev_stop-cmdtable
		dc.w	dev_start-cmdtable
		dc.w	dev_flush-cmdtable
		dc.w	dev_motor-cmdtable
		dc.w	dev_seek-cmdtable
		dc.w	dev_readwrite-cmdtable
		dc.w	dev_remove-cmdtable
		dc.w	dev_changenum-cmdtable
		dc.w	dev_changestate-cmdtable
		dc.w	dev_protstatus-cmdtable
		dc.w	dev_rawread-cmdtable
		dc.w	dev_rawwrite-cmdtable
		dc.w	dev_getdrivetype-cmdtable
cmdtable_end

MAX_COMMAND	EQU	(cmdtable_end-cmdtable)/2

dev_update
dev_clear
dev_reset
dev_remove
dev_seek
dev_motor
dev_changenum
dev_changestate
dev_protstatus
		rts

dev_getdrivetype
		moveq	#DRIVE3_5,d0
		move.l	d0,IO_ACTUAL(a1)
		rts

dev_stop
dev_start
dev_flush
		rts

dev_readwrite
		move.l	md_Board(a6),a0
		move.l	a6,-(sp)
		move.l	md_ExecBase(a6),a6
		jsr	_LVODisable(a6)
		move.l	IO_DATA(a1),board_addr-board_start(a0)
		move.l	IO_OFFSET(a1),board_offs-board_start(a0)
		move.l	IO_LENGTH(a1),board_len-board_start(a0)
		move.w	IO_UNIT+2(a1),board_unit-board_start(a0)
		move.w	IO_COMMAND(a1),board_cntrl-board_start(a0)
		move.l	board_len-board_start(a0),IO_ACTUAL(a1)
		move.w	board_cntrl-board_start(a0),d0
		move.b	d0,IO_ERROR(a1)
		jsr	_LVOEnable(a6)
		move.l	(sp)+,a6
		rts




