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

extern void print_str_safe(char *str);
extern void print_char_safe(char);

extern UINT16 set_ipl(UINT16 ipl);

#ifndef TESTING
IO_PORT8 FBASE = (IO_PORT8)0x3FFD00;
#else
IO_PORT8 FBASE[CB_SECTOR];
#endif

int initialize_floppy_driver(void)
{
    int drive_count = 0;
    UINT16 old_ipl = set_ipl(7);

    /* Reset the FDC to track 0 (restore) */
    if (do_fdc_restore_command() == 0)
        return 0; /* If the restore command failed, return with an error */

    /* Initialize the DMA mode */
    *dma_mode = DMA_MODE_READ | DMA_SECTOR_COUNT_REG_SELECT;

    /* Detect the number of drives */
    select_floppy_drive(DRIVE_A, SIDE_0);
    busy_wait();

    /* Issue a command to drive A and wait for it to complete */
    send_command_to_fdc(FDC_CMD_STEPI | FDC_FLAG_STEP_RATE_2);
    busy_wait(); /* Wait for the command to complete */

    if (!FDC_SEEK_ERROR_CHECK(*fdc_access))
        drive_count++; /* Drive A is present */

    /* Repeat the above steps for drive B if necessary */
    select_floppy_drive(DRIVE_B, SIDE_0);
    busy_wait();

    send_command_to_fdc(FDC_CMD_STEPI | FDC_FLAG_STEP_RATE_2);
    busy_wait(); /* Wait for the command to complete */

    if (!FDC_SEEK_ERROR_CHECK(*fdc_access))
        drive_count++; /* Drive B is present */

    if (do_fdc_restore_command() == 0)
        return 0; /* If the restore command failed, return with an error */

    clear_fdc_interrupt();

    /* Consider handling the motor on status here if necessary */

    set_ipl(old_ipl);

    return drive_count;
}

#ifdef TESTING
#include <osbind.h>
#include <stdio.h>

int do_test_run(int track, int sector)
{
    disk_io_request_t io;
    int i;

    io.disk = DRIVE_A;
    io.side = SIDE_0;
    io.track = track;
    io.sector = sector;
    io.buffer_address = FBASE;
    io.n_sector = 0;

    io.operation = DISK_OPERATION_WRITE;
    for (i = 0; i < CB_SECTOR; i++)
        buffer[i] = 1;
    if (!do_disk_operation(&io))
        goto fail;

    io.operation = DISK_OPERATION_READ;
    for (i = 0; i < CB_SECTOR; i++)
        buffer[i] = 0;
    if (!do_disk_operation(&io))
        goto fail;

    for (i = 0; i < CB_SECTOR; i++)
        if (buffer[i] != (char)i)
            goto fail;

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
    int i;
    *dma_mode = DMA_COMMAND_REG_READ;

    while (*fdc_access & FDC_BUSY)
    {
        ;
    }
}

void reset_fdc(void)
{
    /* Issue a restore command to seek to track 00 */
    send_command_to_fdc(restore_command);

    /* Clear the interrupt status if needed */
    clear_fdc_interrupt();

    /* You might want to check the status register to ensure the command completed successfully */
    /* Code to read and verify FDC status goes here */
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

int do_fdc_read_command()
{
    send_command_to_fdc(read_command);
    return !(FDC_READ_ERROR_CHECK(*fdc_access));
}

int do_fdc_write_command()
{
    int status = 1;

    send_command_to_fdc(write_command);
    status = *fdc_access;
    if (status & FDC_WRITE_PROTECT)
    {
        print_str_safe("FDC_WRITE_PROTECT\r\n");
        status = 0;
    }
    else if (status & FDC_LOST_DATA)
    {
        print_str_safe("FDC_LOST_DATA\r\n");
        status = 0;
    }
    else if (status & FDC_RECORD_NOT_FOUND)
    {
        print_str_safe("FDC_LOST_DATA\r\n");
        status = 0;
    }

    return status;
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
    *dma_mode = DMA_COMMAND_REG_WRITE;
    *fdc_access = command;
    /* wait for command to finish */
    busy_wait();
}

int setup_dma_for_rw(disk_selection_t disk, disk_side_t side, int track)
{
    int status;
    select_floppy_drive(disk, side);
    status = seek(track);
    if (status)
        print_str_safe("seek() pass\r\n");
    else
        print_str_safe("seek() failed\r\n");

    return status;
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
        return read_sector(io->sector, (void *)0);
    }

    return 0;
}

int perform_write_operation_to_floppy(disk_io_request_t *io)
{
    int status;
    setup_dma_buffer((void *)io->buffer_address);
    if (setup_dma_for_rw(io->disk, io->side, io->track))
    {
        status = write_sector(io->sector, (void *)0);
        if (status == 0)
        {
            print_str_safe("write failed\r\n");
        }
    }

    return !status;
}

int do_disk_operation(disk_io_request_t *io)
{
    initialize_dma_buffer(io->operation);

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

void set_dma_length(UINT16 sectors)
{
    /* Length is specified in sectors for the DMA, so for 512 bytes, we set it to 1 */
    *DMA_SECTOR_COUNT_HIGH = (sectors >> 8) & 0xFF; /* High byte of sector count */
    *DMA_SECTOR_COUNT_LOW = sectors & 0xFF;         /* Low byte of sector count */
}

void initialize_dma_buffer(disk_operation_t operation)
{
    *WDC_DMA_BASE_HIGH = ((UINT32)FBASE >> 16) & 0xFF; /* Set DMA base address high byte */
    *WDC_DMA_BASE_MID = ((UINT32)FBASE >> 8) & 0xFF;   /* Set DMA base address middle byte */
    *WDC_DMA_BASE_LOW = (UINT32)FBASE & 0xFF;          /* Set DMA base address low byte */

    /* Set the DMA length for one sector */
    *DMA_SECTOR_COUNT_HIGH = 0;            /* assuming one sector */
    *DMA_SECTOR_COUNT_LOW = SECTOR_LENGTH; /* 1 sector = 512 bytes */

    /* Also make sure the DMA is in the correct mode for reading or writing */
    *dma_mode = operation ? DMA_MODE_READ : DMA_MODE_WRITE;
}

void clear_fdc_interrupt(void)
{
    /* Read the status register to acknowledge and clear any pending interrupt */
    UINT8 status = *fdc_access;

    /* 0x10 == disable motor off */
    send_command_to_fdc(FDC_FLAG_FORCE_INTERRUPT | 0x10);

    busy_wait();
}