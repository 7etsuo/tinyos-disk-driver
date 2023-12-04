#include "TYPES.H"
#include <osbind.h>
#include <stdio.h>

#define CB_SECTOR 512
#define MAX_TRACK 40
#define MAX_SECTOR 9

typedef enum
{
    DISK_OPERATION_READ,
    DISK_OPERATION_WRITE
} disk_operation_t;

typedef enum
{
    DRIVE_A,
    DRIVE_B
} disk_selection_t;

typedef enum
{
    SIDE_0,
    SIDE_1
} disk_side_t;

typedef enum
{
    IOA = 0xE,
    IOB = 0xF
} psg_io_port_t;

typedef enum
{
    PSG_CHANNEL_A_FREQ_LOW,
    PSG_CHANNEL_A_FREQ_HIGH,
    PSG_CHANNEL_B_FREQ_LOW,
    PSG_CHANNEL_B_FREQ_HIGH,
    PSG_CHANNEL_C_FREQ_LOW,
    PSG_CHANNEL_C_FREQ_HIGH,
    PSG_NOISE_FREQ,
    PSG_MIXER_CONTROL,
    PSG_CHANNEL_A_AMP_CONTROL,
    PSG_CHANNEL_B_AMP_CONTROL,
    PSG_CHANNEL_C_AMP_CONTROL,
    PSG_ENVELOPE_PERIOD_HIGH,
    PSG_ENVELOPE_PERIOD_LOW,
    PSG_ENVELOPE_SHAPE,
    PSG_PORT_A_CONTROL,
    PSG_PORT_B
} psg_sel_t;

typedef enum
{
    /* Bit 0 - Side select 0: side1, 1: side0 */
    SIDE_SELECT_0 = 1 << 0,
    /* Bit 1 - Drive A: 0=on, 1=off */
    DRIVE_A_DISABLE = 1 << 1,
    /* Bit 2 - Drive B: 0=on, 1=off */
    DRIVE_B_DISABLE = 1 << 2,
    /* Bit 3 - Printer Select In */
    PRINTER_SELECT_IN = 1 << 3,
    /* Bit 4 - Reset DSP: 0=no, 1=reset */
    DSP_RESET_ENABLE = 1 << 4,
    /* Bit 5 - Centronics Strobe */
    CENTRONICS_STROBE = 1 << 5,
    /* Bit 6 - Internal Speaker: 0=on, 1=off */
    SPEAKER_DISABLE = 1 << 6,
    /* Bit 7 - Reset IDE: 0=no, 1=reset */
    IDE_RESET_ENABLE = 1 << 7
} psg_port_a_control_t;

typedef enum
{
    /* Bit 0 - Channel A Tone (1=Off) */
    CHANNEL_A_TONE = 1 << 0,
    /* Bit 1 - Channel B Tone (1=Off) */
    CHANNEL_B_TONE = 1 << 1,
    /* Bit 2 - Channel C Tone (1=Off) */
    CHANNEL_C_TONE = 1 << 2,
    /* Bit 3 - Channel A Noise (1=Off) */
    CHANNEL_A_NOISE = 1 << 3,
    /* Bit 4 - Channel B Noise (1=Off) */
    CHANNEL_B_NOISE = 1 << 4,
    /* Bit 5 - Channel C Noise (1=Off) */
    CHANNEL_C_NOISE = 1 << 5,
    /* Bit 6 - Port A IN/OUT (1=Output) */
    PORT_A_IN_OUT = 1 << 6,
    /* Bit 7 - Port B IN/OUT (1=Output) */
    PORT_B_IN_OUT = 1 << 7
} psg_mixer_control_t;

IO_PORT8_RO psg_reg_read = (IO_PORT8_RO)0xFF8800;
IO_PORT8 psg_reg_select = (IO_PORT8)0xFF8800;
IO_PORT8 psg_reg_write = (IO_PORT8)0xFF8802;

typedef enum
{
    /* Bit 0 - Busy: 1 when the 177x is busy, 0 when it is free for CPU commands */
    FDC_BUSY = 1 << 0,

    /* Bit 1 - Index / Data Request: High during index pulse for Type I commands, signals CPU for data handling in Type
     * II and III commands */
    FDC_INDEX_DATA_REQUEST = 1 << 1,

    /* Bit 2 - Track Zero / Lost Data: Indicates track zero position after Type I commands or lost data due to slow CPU
     * response for Type II and III commands */
    FDC_TRACK_ZERO_LOST_DATA = 1 << 2,

    /* Bit 3 - CRC Error: Indicates CRC mismatch in data. Can happen from things other than magnetic errors */
    FDC_CRC_ERROR = 1 << 3,

    /* Bit 4 - Record Not Found: Set if the FDC can't find the requested track, sector, or side */
    FDC_RECORD_NOT_FOUND = 1 << 4,

    /* Bit 5 - Spin-up / Record Type: Indicates motor spin-up status for Type I commands and data mark type for Type II
     * and III commands */
    FDC_SPIN_UP_RECORD_TYPE = 1 << 5,

    /* Bit 6 - Write Protect: High during writes if the disk is write-protected */
    FDC_WRITE_PROTECT = 1 << 6,

    /* Bit 7 - Motor On: Indicates if the drive motor is on or off */
    FDC_MOTOR_ON = 1 << 7
} wdc_fdc_access_t;

#define FDC_RESTORE_ERROR_CHECK(fdc_access) (!((fdc_access) & FDC_TRACK_ZERO_LOST_DATA))
#define FDC_SEEK_ERROR_CHECK(fdc_access) ((fdc_access) & FDC_RECORD_NOT_FOUND)
#define FDC_WRITE_ERROR_CHECK(fdc_access)                                                                              \
    ((fdc_access) & (FDC_WRITE_PROTECT | FDC_TRACK_ZERO_LOST_DATA | FDC_RECORD_NOT_FOUND))
#define FDC_READ_ERROR_CHECK(fdc_access)                                                                               \
    ((fdc_access) & (FDC_CRC_ERROR | FDC_TRACK_ZERO_LOST_DATA | FDC_RECORD_NOT_FOUND))

typedef enum
{
    DMA_ERROR_STATUS = 1 << 0,
    SECTOR_COUNT_STATUS = 1 << 1,

    /* Bit 2 - Condition of FDC DATA REQUEST signal */
    DATA_REQUEST_CONDITION = 1 << 2
} wdc_dma_status_t;

typedef enum
{
    /* Bit 0 - Unused, must be set to 0 */

    /* Bit 1 - Controls CA1 output (FDC A0 line and HDC command block start) */
    CA1_CONTROL = 1 << 1,

    /* Bit 2 - Controls CA2 output (FDC A1 line) */
    CA2_CONTROL = 1 << 2,

    /* Bit 3 - Selects HDCS* or FDCS* chip-select (1: HDCS*, 0: FDCS*) */
    HDCS_FDCS_SELECT = 1 << 3,

    /* Bit 4 - Select DMA internal sect cnt or HDC/FDC external */
    /* controller registers (1: internal, 0: external) */
    SECTOR_COUNT_REG_SELECT = 1 << 4,

    /* Bit 5 - Reserved, must be set to 0 */
    /* Bit 6 - Unused, set to 0 (historically for DMA enable/disable) */

    /* Bit 7 - Sets data transfer direction  */
    /* (1: write to controller, 0: read from controller) */
    TRANSFER_DIRECTION = 1 << 7,
} wdc_dma_mode_t;

IO_PORT16 fdc_access = (IO_PORT16)0xFF8604;
IO_PORT16 dma_mode = (IO_PORT16)0xFFFF8606;
IO_PORT16_RO dma_status = (IO_PORT16_RO)0xFFFF8606;

IO_PORT8 WDC_DMA_BASE_HIGH = (IO_PORT8)0xFFFF8609;
IO_PORT8 WDC_DMA_BASE_MID = (IO_PORT8)0xFFFF860B;
IO_PORT8 WDC_DMA_BASE_LOW = (IO_PORT8)0xFFFF860D;

#define SET_DMA_ADDRESS_HIGH_BYTE(val) (*WDC_DMA_BASE_HIGH = (val))
#define SET_DMA_ADDRESS_MID_BYTE(val) (*WDC_DMA_BASE_MID = (val))
#define SET_DMA_ADDRESS_LOW_BYTE(val) (*WDC_DMA_BASE_LOW = (val))

#define SET_DMA_ADDRESS(address)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        SET_DMA_ADDRESS_HIGH_BYTE((char)((long)address >> 16) & 0xFF);                                                 \
        SET_DMA_ADDRESS_MID_BYTE((char)((long)address >> 8) & 0xFF);                                                   \
        SET_DMA_ADDRESS_LOW_BYTE((char)address & 0xFF);                                                                \
    } while (0)

/* DMA Mode Register Operations : FDC Register Mapping in DMA Mode */
#define DMA_MODE_READ 0x00
#define DMA_MODE_WRITE 0x01

/* DMA Register Addresses for Read Mode */

/* Used for initiating read commands and monitoring the status of reads */
#define DMA_COMMAND_REG_READ (DMA_MODE_READ << 8 | 0x80)

/* The track number from which data is to be read */
#define DMA_TRACK_REG_READ (DMA_MODE_READ << 8 | 0x82)

/* Specifying the sector number from which data will be read */
#define DMA_SECTOR_REG_READ (DMA_MODE_READ << 8 | 0x84)

/* Data read from the disk is available here for the CPU to process */
#define DMA_DATA_REG_READ (DMA_MODE_READ << 8 | 0x86)

/* DMA Register Addresses for Write Mode */

/* Used for initiating write commands and monitoring the status of writes */
#define DMA_COMMAND_REG_WRITE (DMA_MODE_WRITE << 8 | 0x80)

/* The track number to which data will be written */
#define DMA_TRACK_REG_WRITE (DMA_MODE_WRITE << 8 | 0x82)

/* Specifying the sector number where data will be written on the disk */
#define DMA_SECTOR_REG_WRITE (DMA_MODE_WRITE << 8 | 0x84)

/* Data to be written to the disk is placed here */
#define DMA_DATA_REG_WRITE (DMA_MODE_WRITE << 8 | 0x86)

/* Holds the count of bytes to be written. Note: This register is write-only */
#define DMA_COUNT_REG_WRITE (DMA_MODE_WRITE << 8 | 0x90)

/* type I command flags */

/* Restore command base */
#define FDC_CMD_RESTORE 0x00
/* Seek command base */
#define FDC_CMD_SEEK 0x10

/* Stepping rate flag r0, corresponds to bit 0 */
#define FDC_FLAG_R0 0x01
/* Stepping rate flag r1, corresponds to bit 1 */
#define FDC_FLAG_R1 0x02
/* Verify flag 'V', corresponds to bit 2 */
#define FDC_FLAG_V 0x04
/* Motor On flag 'h', corresponds to bit 3 */
#define FDC_FLAG_H 0x08

/* type II command flags */

/* Read Sector command base */
#define FDC_CMD_READ 0x80
/* Write Sector command base */
#define FDC_CMD_WRITE 0xA0
/* Data Address Mark 'a0', corresponds to bit 0 */
#define FDC_FLAG_DATA_ADDR_MARK 0x01
/* Write Precompensation 'p', corresponds to bit 1 */
#define FDC_FLAG_WRITE_PRECOMP 0x02
/* Delay flag 'e', corresponds to bit 2 */
#define FDC_FLAG_DELAY 0x04
/* Motor On flag 'h', corresponds to bit 3 */
#define FDC_FLAG_MOTOR_ON 0x08
/* Multiple Sector flag 'm', corresponds to bit 4 */
#define FDC_FLAG_MULTIPLE_SECT 0x10

const UINT8 restore_command = FDC_FLAG_H | FDC_CMD_RESTORE | FDC_FLAG_R1;
const UINT8 seek_command = FDC_FLAG_H | FDC_CMD_SEEK | FDC_FLAG_R1;
const UINT8 read_command = FDC_CMD_READ | FDC_FLAG_MOTOR_ON;
const UINT8 write_command = FDC_CMD_WRITE | FDC_FLAG_MOTOR_ON | FDC_FLAG_WRITE_PRECOMP;
const UINT8 write_deleted_data_command =
    FDC_CMD_WRITE | FDC_FLAG_MOTOR_ON | FDC_FLAG_WRITE_PRECOMP | FDC_FLAG_DATA_ADDR_MARK;

/*
    NOTE: Typically if you wanted to simulate the behavior where the motor spin-up sequence is skipped
            (assuming the motor is already running), then you would include the `h` flag.
            This is often the case when you have issued a command recently and you know the motor has not had time
            to spin down.
*/
/* FDC_FLAG_R1 => 2000 cycles */

typedef struct
{
    /* Type of operation (e.g., READ, WRITE) */
    disk_operation_t operation;
    /* Disk selection (e.g., DRIVE_A, DRIVE_B) */
    disk_selection_t disk;
    /* Disk side (e.g., SIDE_0, SIDE_1) */
    disk_side_t side;
    /* Track number for the operation */
    int track;
    /* Starting sector number for the operation */
    int sector;
    /* Pointer to the data buffer for read/write */
    void *buffer_address;
    /* Number of sectors to read/write (NOT IMPLEMENTED)*/
    int n_sector;
} disk_io_request_t;

int initialize_floppy_driver(void);
void handle_floppy_interrupt(void);
void setup_dma_buffer(void *buffer_address);
int setup_dma_for_rw(disk_selection_t disk, disk_side_t side, int track);

void select_floppy_drive(disk_selection_t drive, disk_side_t side);
void send_command_to_fdc(UINT8 command);
void busy_wait(void);

char get_fdc_track(void);
void set_fdc_sector(int sector);
void set_fdc_track(int track);

int do_fdc_restore_command(void);
int do_fdc_seek_command(void);
int do_fdc_read_command(void *buffer_address);
int do_fdc_write_command(const void *buffer_address);

int seek(int track);
int read_sector(int sector, void *buffer_address);
int write_sector(int sector, const void *buffer_address);

int perform_disk_operation(disk_io_request_t *disk_io_req);
int perform_read_operation_from_floppy(disk_io_request_t *io);
int perform_write_operation_to_floppy(disk_io_request_t *io);


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

    if (!perform_disk_operation(&io))
    {
        goto fail;
    }

    for (i = 0; i < CB_SECTOR; i++)
    {
        buffer[i] = 0;
    }

    io.operation = DISK_OPERATION_READ;
    if (!perform_disk_operation(&io))
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

void select_floppy_drive(disk_selection_t drive, disk_side_t side)
{
    UINT8 register_state;

    /* UINT16 orig_ipl = set_ipl(7);  need to mask both MFP IRQ and VBL IRQ */
    *psg_reg_select = PSG_PORT_A_CONTROL;
    register_state = (*psg_reg_read) & 0xF8;

    /* 0:on,1:off */
    register_state |= drive == DRIVE_A ? DRIVE_B_DISABLE : DRIVE_A_DISABLE;
    register_state |= side == SIDE_SELECT_0;

    *psg_reg_write = register_state;
    /*   (void)set_ipl(orig_ipl); */
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
    else { 
        /* read or write was good ; do queue stuff */
        ;
    }
    /* Detect disk change and handle it */
    /* Reset hardware or retry operation */
}
