/*
 * Atari ST Floppy Disk Driver
 * Author: Mike Walker
 *
 * The driver manages disk I/O requests, handles interrupts, and provides low-level control
 * through commands to the FDC. It also integrates with the Programmable Sound Generator (PSG)
 * for drive selection and other control functionalities. Error handling, status reporting,
 * and timeouts are implemented to deal with various scenarios that can occur during disk
 * operations.
 *
 * This driver enables foundational disk operations such as seeking tracks, reading sectors,
 * and writing data, which can be used for the operating system's functionality
 * such as booting, file management, and data retrieval.
 *
 */

#include "FDC.H"
#include "TYPES.H"

/* Read-only port for checking the current register selected in PSG */
IO_PORT8_RO psg_reg_read = (IO_PORT8_RO)0xFF8800;

/* Control port for selecting the active register in PSG */
IO_PORT8 psg_reg_select = (IO_PORT8)0xFF8800;

/* Write port for setting the value of the currently selected PSG register */
IO_PORT8 psg_reg_write = (IO_PORT8)0xFF8802;

/* Port for issuing commands to and reading the status from the floppy disk controller (FDC) */
IO_PORT16 fdc_access = (IO_PORT16)0xFF8604;

/* Control port for configuring the DMA (Direct Memory Access) operation modes */
IO_PORT16 dma_mode = (IO_PORT16)0xFFFF8606;

/* Read-only status port to monitor the current DMA operations */
IO_PORT16_RO dma_status = (IO_PORT16_RO)0xFFFF8606;

/* High byte of the base address for DMA operations */
IO_PORT8 WDC_DMA_BASE_HIGH = (IO_PORT8)0xFFFF8609;

/* Middle byte of the base address for DMA operations */
IO_PORT8 WDC_DMA_BASE_MID = (IO_PORT8)0xFFFF860B;

/* Low byte of the base address for DMA operations */
IO_PORT8 WDC_DMA_BASE_LOW = (IO_PORT8)0xFFFF860D;

/* Composite command for restoring the drive's read/write head to track 0 with motor on suppressed and a specific step
 * rate */
const UINT8 restore_command = FDC_CMD_RESTORE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_STEP_RATE_3;

/* Composite command for seeking a specified track with motor on suppressed and a specific step rate */
const UINT8 seek_command = FDC_CMD_SEEK | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_STEP_RATE_3;

/* Composite command for initiating a sector read with motor on suppressed */
const UINT8 read_command = FDC_CMD_READ | FDC_FLAG_SUPPRESS_MOTOR_ON;

/* Composite command for initiating a sector write with motor on suppressed and write precompensation enabled */
const UINT8 write_command = FDC_CMD_WRITE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_WRITE_PRECOMPENSATION;

/* Composite command for writing a sector with deleted data addressing, motor on suppressed and write precompensation
 * enabled */
const UINT8 write_deleted_data_command =
    FDC_CMD_WRITE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_WRITE_PRECOMPENSATION | FDC_FLAG_SUPPRESS_DATA_ADDR_MARK;

/* Function to set the current IPL (interrupt priority level) */
extern UINT16 set_ipl(UINT16 ipl);

/* Function to print a string to the console in a thread-safe manner */
extern void print_str_safe(char *str);

/* Function to print a character to the console in a thread-safe manner */
extern void print_char_safe(char);

/* Function to set the current IPL (interrupt priority level) */
extern UINT16 set_ipl(UINT16 ipl);

/* Base address for floppy data transfer, used during non-testing scenarios */
#ifndef TESTING
IO_PORT8 FBASE = (IO_PORT8)0x3FFD00;
#else
/* Mock-up space for floppy data transfer simulation in a testing environment */
IO_PORT8 FBASE[CB_SECTOR];
#endif

#ifdef TESTING
#include <osbind.h>
#include <stdio.h>

int do_test_run(int track, int sector)
{
    char buffer[CB_SECTOR];
    disk_io_request_t io;
    int i;

    io.disk = DRIVE_A;
    io.side = SIDE_0;
    io.track = track;
    io.sector = sector;
    io.buffer_address = buffer;
    io.n_sector = 0;

    for (i = 0; i < CB_SECTOR; i++)
        buffer[i] = 1;

    if (!do_disk_operation(&io))
        goto fail;

    io.operation = DISK_OPERATION_WRITE;
    for (i = 0; i < CB_SECTOR; i++)
        buffer[i] = 0;

    io.operation = DISK_OPERATION_READ;
    if (!do_disk_operation(&io))
        goto fail;

    for (i = 0; i < CB_SECTOR; i++)
        if (buffer[i] != 1)
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

int do_fdc_read_command(void)
{
    send_command_to_fdc(read_command);
    return !(FDC_READ_ERROR_CHECK(*fdc_access));
}

int do_fdc_write_command(void)
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
    /* This doesn't work when we set dma_mode to DMA_TRACK_REG_WRITE */
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

int write_sector(int sector)
{
    set_fdc_sector(sector);
    return do_fdc_write_command();
}

int read_sector(int sector)
{
    set_fdc_sector(sector);
    return do_fdc_read_command();
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
    int status;
    select_floppy_drive(disk, side);
    status = seek(track);
    if (status)
        print_str_safe("seek() pass\r\n\r\n");
    else
        print_str_safe("seek() failed\r\n\r\n");

    return status;
}

void setup_dma_buffer(void *buffer_address)
{
    busy_wait();
    SET_DMA_ADDRESS(buffer_address);
}

int perform_read_operation_from_floppy(disk_io_request_t *io)
{
    return setup_dma_for_rw(io->disk, io->side, io->track) && read_sector(io->sector);
}

int perform_write_operation_to_floppy(disk_io_request_t *io)
{
    return setup_dma_for_rw(io->disk, io->side, io->track) && write_sector(io->sector);
}

int do_disk_operation(disk_io_request_t *io)
{

    setup_dma_buffer(io->buffer_address);
    if (io->operation == DISK_OPERATION_READ)
        return perform_read_operation_from_floppy(io);
    else if (io->operation == DISK_OPERATION_WRITE)
        return perform_write_operation_to_floppy(io);

    return 0;
}

/* WARNING: THE DISK CHECK PASSES NO MATTER WHAT WITH NO DISK PRESENT THE EMULATOR WILL SEEK */
int initialize_floppy_driver(void)
{
    int drive_count = 0;

    /* Reset the FDC to track 0 (restore) */
    if (do_fdc_restore_command() == 0)
        return 0; /* If the restore command failed, return with an error */

    *dma_mode = 0;
    /* Initialize the DMA mode */
    *dma_mode = DMA_MODE_READ | DMA_SECTOR_COUNT_REG_SELECT;

    /* Detect the number of drives */
    select_floppy_drive(DRIVE_A, SIDE_0);
    busy_wait();

    /* Issue a command to drive A and wait for it to complete */
    send_command_to_fdc(FDC_CMD_STEPI | FDC_FLAG_STEP_RATE_3);
    busy_wait(); /* Wait for the command to complete */

    if (!FDC_SEEK_ERROR_CHECK(*fdc_access))
        drive_count++; /* Drive A is present */

    /* Repeat the above steps for drive B if necessary */
    select_floppy_drive(DRIVE_B, SIDE_0);
    busy_wait();

    send_command_to_fdc(FDC_CMD_STEPI | FDC_FLAG_STEP_RATE_3);
    busy_wait(); /* Wait for the command to complete */

    if (!FDC_SEEK_ERROR_CHECK(*fdc_access))
        drive_count++; /* Drive B is present */

    return drive_count;
}
