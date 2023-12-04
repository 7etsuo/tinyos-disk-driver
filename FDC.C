#include "TYPES.H"

#include "FDC.H"

IO_PORT8_RO psg_reg_read = (IO_PORT8_RO)0xFF8800;
IO_PORT8 psg_reg_select = (IO_PORT8)0xFF8800;
IO_PORT8 psg_reg_write = (IO_PORT8)0xFF8802;

IO_PORT16 fdc_access = (IO_PORT16)0xFF8604;
IO_PORT16 dma_mode = (IO_PORT16)0xFFFF8606;
IO_PORT16_RO dma_status = (IO_PORT16_RO)0xFFFF8606;

IO_PORT8 WDC_DMA_BASE_HIGH = (IO_PORT8)0xFFFF8609;
IO_PORT8 WDC_DMA_BASE_MID = (IO_PORT8)0xFFFF860B;
IO_PORT8 WDC_DMA_BASE_LOW = (IO_PORT8)0xFFFF860D;

const UINT8 restore_command = FDC_CMD_RESTORE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_STEP_RATE_2;
const UINT8 seek_command = FDC_CMD_SEEK | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_STEP_RATE_2;
const UINT8 read_command = FDC_CMD_READ | FDC_FLAG_SUPPRESS_MOTOR_ON;
const UINT8 write_command = FDC_CMD_WRITE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_WRITE_PRECOMPENSATION;
const UINT8 write_deleted_data_command =
    FDC_CMD_WRITE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_WRITE_PRECOMPENSATION | FDC_FLAG_SUPPRESS_DATA_ADDR_MARK;

extern UINT16 set_ipl(UINT16 ipl);

#ifdef TESTING
#include <osbind.h>
#include <stdio.h>

int do_test_run(int track, int sector)
{
    char buffer[CB_SECTOR];
    disk_io_request_t io;
    int i;

    io.operation = DISK_OPERATION_WRITE;
    io.disk = DRIVE_A;
    io.side = SIDE_0;
    io.track = track;
    io.sector = sector;
    io.buffer_address = buffer;
    io.n_sector = 0;

    for (i = 0; i < CB_SECTOR; i++)
    {
        buffer[i] = i;
    }

    if (!do_disk_operation(&io))
    {
        goto fail;
    }

    for (i = 0; i < CB_SECTOR; i++)
    {
        buffer[i] = 0;
    }

    io.operation = DISK_OPERATION_READ;
    if (!do_disk_operation(&io))
    {
        goto fail;
    }

    for (i = 0; i < CB_SECTOR; i++)
    {
        if (buffer[i] != (char)i)
        {
            goto fail;
        }
    }

    printf("pass\n");
    printf("TEST: track %d sector %d\n\n", track, sector);
    return 1;
fail:
    printf("fail\n");
    printf("TEST: track %d sector %d\n\n", track, sector);
    return 0;
}

int main(void)
{
    int i;
    long orig_ssp = Super(0);

    if (initialize_floppy_driver() == 0)
        goto fail;

    for (i = 1; i != MAX_SECTOR; i++)
    {
        if (!do_test_run(0, i))
            goto fail;
    }

    for (i = 1; i != MAX_SECTOR; i++)
    {
        if (!do_test_run(1, i))
            goto fail;
    }

fail:
    Super(orig_ssp);
    return 0;
}
#endif /* TESTING */
void select_floppy_drive(disk_selection_t drive, disk_side_t side)
{
    UINT8 register_state;
#ifndef TESTING
    UINT16 orig_ipl = set_ipl(7);
#endif
    *psg_reg_select = PSG_PORT_A_CONTROL;
    register_state = (*psg_reg_read) & 0xF8;

    /* 0:on,1:off */
    register_state |= drive == DRIVE_A ? DRIVE_B_DISABLE : DRIVE_A_DISABLE;
    register_state |= side == SIDE_SELECT_0;

    *psg_reg_write = register_state;
#ifndef TESTING
    (void)set_ipl(orig_ipl);
#endif
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

int do_disk_operation(disk_io_request_t *io)
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
    if (FDC_READ_ERROR_CHECK(fdc_stat) || (dma_stat & DMA_OK_ERROR_STATUS))
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
