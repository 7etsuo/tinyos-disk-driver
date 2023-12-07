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

const UINT8 restore_command = FDC_CMD_RESTORE | FDC_FLAG_STEP_RATE_2;
const UINT8 seek_command = FDC_CMD_SEEK | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_STEP_RATE_2;
const UINT8 read_command = FDC_CMD_READ | FDC_FLAG_SUPPRESS_MOTOR_ON;
const UINT8 write_command = FDC_CMD_WRITE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_WRITE_PRECOMPENSATION;
const UINT8 write_deleted_data_command =
    FDC_CMD_WRITE | FDC_FLAG_SUPPRESS_MOTOR_ON | FDC_FLAG_WRITE_PRECOMPENSATION | FDC_FLAG_SUPPRESS_DATA_ADDR_MARK;

extern UINT16 set_ipl(UINT16 ipl);

UINT16 *const flock = (UINT16 *)0x000496;
UINT8 *const fstatus = (UINT8 *)0x00049C;

UINT8 *const fseek = (UINT8 *)0x00049E;
UINT8 *const fzero = (UINT8 *)0x00049F;
UINT8 *const fread = (UINT8 *)0x0004A0;
UINT8 *const fwrite = (UINT8 *)0x0004A1;
UINT8 *const interrupt_processed = (UINT8 *)0x0004A2;

IO_PORT8 FDISK_BASE = (IO_PORT8)0x3FFD00;

extern void print_str_safe(char *);
extern void print_char_safe(char);
extern void *safe_memcpy(void *dest, const void *src, UINT32 n);

int initialize_floppy_driver(void)
{
    int drive_count = 0;
    UINT32 i;

    UINT16 orig_ipl = set_ipl(7);

    *flock = 1;
    /* Reset the FDC to track 0 (restore) */
    do_fdc_restore_command();

    *dma_mode = 0;
    /* Initialize the DMA mode */
    *dma_mode = DMA_MODE_READ | DMA_SECTOR_COUNT_REG_SELECT;

    /* Detect the number of drives */
    select_floppy_drive(DRIVE_A, SIDE_0);
    busy_wait();

    /* Issue a command to drive A and wait for it to complete */
    send_command_to_fdc(FDC_CMD_STEPI | FDC_FLAG_STEP_RATE_2);
    busy_wait(); /* Wait for the command to complete */

    if (!FDC_SEEK_ERROR_CHECK(*fdc_access))
    {
        drive_count++; /* Drive A is present */
    }

    /* Repeat the above steps for drive B */
    select_floppy_drive(DRIVE_B, SIDE_0);
    busy_wait();

    send_command_to_fdc(FDC_CMD_STEPI | FDC_FLAG_STEP_RATE_2);
    busy_wait(); /* Wait for the command to complete */

    if (!FDC_SEEK_ERROR_CHECK(*fdc_access))
    {
        drive_count++; /* Drive B is present */
    }

    do_fdc_restore_command();

    *flock = 0;
    set_ipl(orig_ipl);

    return drive_count;
}

void clear_dma_status(void)
{
    /* clear the status */
    (void)*dma_status;
}

void select_floppy_drive(disk_selection_t drive, disk_side_t side)
{
    UINT8 register_state;

    UINT16 orig_ipl = set_ipl(7);

    *psg_reg_select = PSG_PORT_A_CONTROL;
    register_state = (*psg_reg_read) & 0xF8;

    /* 0:on,1:off */
    if (drive == DRIVE_A)
    {
        register_state &= ~DRIVE_A_DISABLE; /* Clear bit to enable drive A */
        register_state |= DRIVE_B_DISABLE;  /* Set bit to disable drive B */
    }
    else
    {
        register_state |= DRIVE_A_DISABLE;  /* Set bit to disable drive A */
        register_state &= ~DRIVE_B_DISABLE; /* Clear bit to enable drive B */
    }

    register_state |= side == SIDE_SELECT_0;

    *psg_reg_write = register_state;

    (void)set_ipl(orig_ipl);
}

void busy_wait_with_timeout(UINT32 timeout)
{
    UINT32 timer = 0;
    while ((*fdc_access) & FDC_BUSY && timer < timeout)
    {
        timer++;
    }
    if (timer >= timeout)
    {
        /* Logs timeout error, handle accordingly */
        print_str_safe("FDC Busy Timeout.\r\n");
    }
}

void busy_wait(void)
{
    int i = 32;
    while (i--)
        ;
}

void do_fdc_restore_command(void)
{
    *fzero = 0;
    send_command_to_fdc(restore_command);
}

void do_fdc_seek_command(void)
{
    *fseek = 1;
    send_command_to_fdc(seek_command);
}

void do_fdc_read_command()
{
    *fread = 1;
    send_command_to_fdc(read_command);
}

void do_fdc_write_command()
{
    *fwrite = 1;
    send_command_to_fdc(write_command);
}

void set_fdc_track(int track)
{
    /* No clue why this doesn't work when I set dma_mode to DMA_TRACK_REG_WRITE */
    UINT16 orig_ipl = set_ipl(7);
    *dma_mode = DMA_DATA_REG_WRITE;
    *fdc_access = track;
    busy_wait();
    set_ipl(orig_ipl);
}

void set_fdc_sector(int sector)
{
    UINT16 orig_ipl = set_ipl(7);
    *dma_mode = DMA_SECTOR_REG_WRITE;
    *fdc_access = sector;
    busy_wait();
    set_ipl(orig_ipl);
}

char get_fdc_track(void)
{
    char retval;
    UINT16 orig_ipl = set_ipl(7);
    *dma_mode = DMA_TRACK_REG_READ;
    busy_wait();
    retval = (char)*fdc_access;
    set_ipl(orig_ipl);

    clear_dma_status();

    return retval;
}

void seek(int track)
{
    UINT16 orig_ipl;
    set_fdc_track(track);

    orig_ipl = set_ipl(7);
    do_fdc_seek_command();
    *flock = 0;
    set_ipl(orig_ipl);

    print_str_safe("seek start\r\n");

    while (waitfstatus() != SEEK_DONE)
        ;

    *flock = 1;
    *fstatus = FDC_BUSY;

    print_str_safe("seek done\r\n");
}

void write_sector(int sector)
{
    set_fdc_sector(sector);
    do_fdc_write_command();
}

void read_sector(int sector)
{
    set_fdc_sector(sector);
    do_fdc_read_command();
}

void send_command_to_fdc(UINT8 command)
{
    UINT32 i = 5000L;
    UINT16 orig_ipl = set_ipl(7);
    busy_wait_with_timeout(FDC_TIMEOUT);
    *dma_mode = DMA_COMMAND_REG_WRITE;
    *fdc_access = command;
    /* wait for command to write */
    busy_wait_with_timeout(FDC_TIMEOUT);
    set_ipl(orig_ipl);
}

void setup_dma_for_rw(disk_selection_t disk, disk_side_t side, int track)
{
    select_floppy_drive(disk, side);
    seek(track);
}

void setup_dma_buffer(void *buffer_address)
{
    SET_DMA_ADDRESS(buffer_address);
}

void perform_read_operation_from_floppy(disk_io_request_t *io)
{
    setup_dma_buffer((void *)io->buffer_address);
    setup_dma_for_rw(io->disk, io->side, io->track);
    read_sector(io->sector);
}

void perform_write_operation_to_floppy(disk_io_request_t *io)
{
    setup_dma_buffer((void *)io->buffer_address);
    setup_dma_for_rw(io->disk, io->side, io->track);
    write_sector(io->sector);
    busy_wait_with_timeout(10000);
}

void wait_for_interrupt_processed_signal()
{
    while (!*interrupt_processed)
    {
        ; /* Consider adding a delay here to prevent a tight loop which can consume CPU */
    }
    *interrupt_processed = 0; /* Clear the signal */
}

void do_disk_operation(disk_io_request_t *io)
{

    *flock = 1; /* Lock the operation */

    init_floppy_flags();

    *fstatus = 0; /* Set the initial status */

    if (io->operation == DISK_OPERATION_WRITE)
    {
        perform_write_operation_to_floppy(io);
    }
    else if (io->operation == DISK_OPERATION_READ)
    {
        perform_read_operation_from_floppy(io);
    }
    else
    {
        return;
    }

    /* Wait for the interrupt handler to signal that the operation is complete */
    wait_for_interrupt_processed_signal();

    /* Clear the lock and check the status */
    *flock = 0;
    if (*fstatus & (SEEK_DONE | READ_DONE | WRITE_DONE))
    {
        print_str_safe("Operation Succeeded\r\n");
        /* Operation succeeded */
    }
    else
    {
        print_str_safe("Operation Failed\r\n");
        /* Operation failed, handle error */
    }
}

void handle_floppy_interrupt(void)
{
    /* Read the FDC status register */
    UINT16 fdc_stat = *fdc_access;

    /* Read DMA status to clear any interrupts */
    UINT16 dma_stat = *dma_status;

    /* Acknowledge the interrupt - Assuming that these operations clear the interrupt request */
    (void)*fdc_access;
    clear_dma_status();

    /* Check if the FDC is busy, and if not, handle errors and set the completion flag */
    if (!(fdc_stat & FDC_BUSY))
    {
        if (fdc_stat & FDC_LOST_DATA)
        {
            *fstatus |= LOST_DATA;
        }
        else if (fdc_stat & FDC_CRC_ERROR)
        {
            *fstatus |= CRC_ERROR;
        } /* Add other error condition checks here... */
        else
        {
            /* Check if any operation flags are set, then clear them and update fstatus */
            if (*fseek)
            {
                *fseek = 0;
                *fstatus |= SEEK_DONE;
            }
            if (*fread)
            {
                *fread = 0;
                *fstatus |= READ_DONE;
            }
            if (*fwrite)
            {
                *fwrite = 0;
                *fstatus |= WRITE_DONE;
            }
        }
    }
    /* Signal that the interrupt has been processed and the status has been updated */

    *interrupt_processed = 1;
    /* This could be a condition variable or semaphore */
}

void init_floppy_flags(void)
{
    *interrupt_processed = 0;
    *fstatus = COMMAND_COMPLETE;
    *fseek = 0;
    *fread = 0;
    *fwrite = 0;
}