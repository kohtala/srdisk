;EMS function codes to be put in AH

EMS_GET_STATUS          equ 40h
EMS_FRAME_SEGMENT       equ 41h ; Ret: BX=frame segment
EMS_FREE_PAGE_COUNT     equ 42h ; Ret: BX=# of unallocated pages
EMS_ALLOCATE            equ 43h ; BX=Pages, Ret: DX=Handle
EMS_MAP_HANDLE_PAGES    equ 44h ; AL=Phys page, BX=Log page, DX=Handle
EMS_DEALLOCATE          equ 45h ; DX=Handle
EMS_GET_VERSION         equ 46h ; Ret: AL=BCD EMM version
EMS_SAVE_PMAP           equ 47h ; Save page map
EMS_RESTORE_PMAP        equ 48h ; Restore page map
EMS_REALLOCATE_PAGES    equ 51h ; BX=# of pages, DX=handle, Ret: BX=# of pages
EMS_MOVE_MEM_REGION     equ 57h ; AL=0 (move)/1 (exchange) DS:SI=region

