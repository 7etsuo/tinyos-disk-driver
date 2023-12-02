#include "FDC.H"
#include "TYPES.H"

extern UINT16 set_ipl(UINT16 ipl);

void select_floppy_drive(disk_selection_t drive, disk_side_t side)
{
    UINT8 register_state;

    UINT16 orig_ipl = set_ipl(7);
    *psg_reg_select = PSG_PORT_A_CONTROL;
    register_state = (*psg_reg_read) & 0xF8;

    /* 0:on,1:off */
    register_state |= drive == DRIVE_A ? DRIVE_B_DISABLE : DRIVE_A_DISABLE;
    register_state |= side == SIDE_SELECT_0;

    *psg_reg_write = register_state;
    (void)set_ipl(orig_ipl);
}

void busy_wait(void)
{
    *dma_mode = DMA_COMMAND_REG_READ;

    while (*fdc_access & FDC_BUSY)
    {
        ;
    }
}

int do_fdc_restore_command(void)
{
    send_command_to_fdc(restore_command);
    return !FDC_RESTORE_ERROR_CHECK(*fdc_access);
}

int do_fdc_seek_command(void)
{
    send_command_to_fdc(seek_command);
    return !(FDC_SEEK_ERROR_CHECK(*fdc_access));
}

int do_fdc_read_command(void *buffer_address)
{
    send_command_to_fdc(read_command);
    return !(FDC_READ_ERROR_CHECK(*fdc_access));
}

int do_fdc_write_command(const void *buffer_address)
{
    send_command_to_fdc(write_command);
    return !(FDC_WRITE_ERROR_CHECK(*fdc_access));
}

void set_fdc_track(int track)
{
    /* No clue why this doesn't work when I set dma_mode to DMA_TRACK_REG_WRITE */
    busy_wait();
    *dma_mode = DMA_DATA_REG_WRITE;
    *fdc_access = track;
}

void set_fdc_sector(int sector)
{
    busy_wait();
    *dma_mode = DMA_SECTOR_REG_WRITE;
    *fdc_access = sector;
}

char get_fdc_track(void)
{
    busy_wait();
    *dma_mode = DMA_TRACK_REG_READ;

    return (char)*fdc_access;
}

int seek(int track)
{
    set_fdc_track(track);
    return (do_fdc_seek_command() != 0 && get_fdc_track() == track);
}

int write_sector(int sector, const void *buffer_address)
{
    set_fdc_sector(sector);
    return do_fdc_write_command(buffer_address);
}

int read_sector(int sector, void *buffer_address)
{
    set_fdc_sector(sector);
    return do_fdc_read_command(buffer_address);
}

void send_command_to_fdc(UINT8 command)
{
    /* Write command and any necessary parameters to the FDC's registers */
    busy_wait();
    *dma_mode = DMA_COMMAND_REG_WRITE;
    *fdc_access = command;
    /* wait for command to finish */
    busy_wait();
}

int setup_dma_for_rw(disk_selection_t disk, disk_side_t side, int track)
{
    select_floppy_drive(disk, side);
    return seek(track);
}

void setup_dma_buffer(void *buffer_address)
{
    busy_wait();
    SET_DMA_ADDRESS(buffer_address);
}

int perform_read_operation_from_floppy(disk_io_request_t *io)
{
    if (setup_dma_for_rw(io->disk, io->side, io->track))
    {
        setup_dma_buffer(io->buffer_address);
        return read_sector(io->sector, io->buffer_address);
    }

    return 0;
}

int perform_write_operation_to_floppy(disk_io_request_t *io)
{

    if (setup_dma_for_rw(io->disk, io->side, io->track))
    {
        setup_dma_buffer(io->buffer_address);
        return write_sector(io->sector, io->buffer_address);
    }

    return 0;
}

int perform_disk_operation(disk_io_request_t *io)
{
    if (io->operation == DISK_OPERATION_READ)
    {
        return perform_read_operation_from_floppy(io);
    }
    else if (io->operation == DISK_OPERATION_WRITE)
    {
        return perform_write_operation_to_floppy(io);
    }

    return 0;
}

int initialize_floppy_driver(void)
{
    select_floppy_drive(DRIVE_A, SIDE_0);
    return do_fdc_restore_command();
}

void handle_floppy_interrupt(void)
{
    UINT16 fdc_stat = *fdc_access;
    UINT16 dma_stat = *dma_status;

    /* Check for errors and handle them */
    if (FDC_READ_ERROR_CHECK(fdc_stat) || (dma_stat & DMA_ERROR_STATUS))
    {
        ;
    }
    else if (FDC_WRITE_ERROR_CHECK(fdc_stat))
    {
        ;
    }
    else
    {
        /* read or write was good ; do queue stuff */
        ;
    }
    /* Detect disk change and handle it */
    /* Reset hardware or retry operation */
}