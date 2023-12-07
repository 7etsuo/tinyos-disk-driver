					xdef	_read_SR,_write_SR

_read_SR:			move.w	sr,d0
					rts

_write_SR:			move.w	4(sp),sr
					rts