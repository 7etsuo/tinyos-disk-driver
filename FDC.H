/*
 * Atari ST Floppy Disk Driver
 * Author: Mike Walker
 *
 * This header contains definitions and function prototypes for controlling the WD1772
 * Floppy Disk Controller (FDC) used in Atari ST computers. It includes configuration
 * of the DMA for data transfer, definitions of command sets for the FDC,
 * and utilities for performing read/write operations on the floppy disk.
 * The driver adheres to the hardware specifications of the Atari ST platform to ensure efficient
 * and correct operations with disk media.
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

#ifndef FDC_H
#define FDC_H

#include "TYPES.H"

#define CB_SECTOR 512
#define MAX_TRACK 40
#define MAX_SECTOR 9

#define FLOPPY_MOTOR_TIMEOUT 1000000 /* Timeout for motor spin-up */
#define FDC_TIMEOUT 15000            /* timeout for FDC */

/* Enumerations for disk operation statuses */
typedef enum
{
    BUSY,             /* FDC is busy with an operation */
    COMMAND_COMPLETE, /* Command has been completed successfully */
    LOST_DATA,        /* Data has been lost during the operation */
    CRC_ERROR,        /* CRC check failed, data may be corrupted */
    RECORD_NOT_FOUND, /* The requested track, sector, or side could not be found */
    WRITE_PROTECT,    /* Attempt to write on a write-protected disk */
    SEEK_DONE,        /* Seeking operation is done */
    BUSY_ZERO,        /* Reserved status */
    WRITE_DONE,       /* Write operation is successfully completed */
    READ_DONE         /* Read operation is successfully completed */
} disk_status_t;

/* Enumerations for specifying the disk operation type */
typedef enum
{
    DISK_OPERATION_READ, /* Read operation */
    DISK_OPERATION_WRITE /* Write operation */
} disk_operation_t;

/* Enumerations for selecting the floppy drive */
typedef enum
{
    DRIVE_A, /* Floppy Drive A */
    DRIVE_B  /* Floppy Drive B */
} disk_selection_t;

/* Enumerations for selecting the side of the floppy disk */
typedef enum
{
    SIDE_0, /* Side 0 of the floppy disk */
    SIDE_1  /* Side 1 of the floppy disk */
} disk_side_t;

/* Enumerations for PSG I/O port addresses */
typedef enum
{
    IOA = 0xE, /* PSG I/O Port A */
    IOB = 0xF  /* PSG I/O Port B */
} psg_io_port_t;

/* Enumerations for selecting PSG registers */
typedef enum
{
    PSG_CHANNEL_A_FREQ_LOW,    /* PSG Channel A Frequency Low */
    PSG_CHANNEL_A_FREQ_HIGH,   /* PSG Channel A Frequency High */
    PSG_CHANNEL_B_FREQ_LOW,    /* PSG Channel B Frequency Low */
    PSG_CHANNEL_B_FREQ_HIGH,   /* PSG Channel B Frequency High */
    PSG_CHANNEL_C_FREQ_LOW,    /* PSG Channel C Frequency Low */
    PSG_CHANNEL_C_FREQ_HIGH,   /* PSG Channel C Frequency High */
    PSG_NOISE_FREQ,            /* PSG Noise Frequency */
    PSG_MIXER_CONTROL,         /* PSG Mixer Control */
    PSG_CHANNEL_A_AMP_CONTROL, /* PSG Channel A Amplitude Control */
    PSG_CHANNEL_B_AMP_CONTROL, /* PSG Channel B Amplitude Control */
    PSG_CHANNEL_C_AMP_CONTROL, /* PSG Channel C Amplitude Control */
    PSG_ENVELOPE_PERIOD_HIGH,  /* PSG Envelope Period High */
    PSG_ENVELOPE_PERIOD_LOW,   /* PSG Envelope Period Low */
    PSG_ENVELOPE_SHAPE,        /* PSG Envelope Shape */
    PSG_PORT_A_CONTROL,        /* PSG Port A Control */
    PSG_PORT_B                 /* PSG Port B */
} psg_sel_t;

/* Control bits for PSG Port A functionality */
typedef enum
{
    SIDE_SELECT_0 = 1 << 0,     /* Select disk side: 0 for side 1, 1 for side 0 */
    DRIVE_A_DISABLE = 1 << 1,   /* Control Drive A: 0 enables, 1 disables */
    DRIVE_B_DISABLE = 1 << 2,   /* Control Drive B: 0 enables, 1 disables */
    PRINTER_SELECT_IN = 1 << 3, /* Printer Select Input */
    DSP_RESET_ENABLE = 1 << 4,  /* Reset Digital Sound Processor: 0 does not reset, 1 resets */
    CENTRONICS_STROBE = 1 << 5, /* Centronics Interface Strobe Signal */
    SPEAKER_DISABLE = 1 << 6,   /* Internal Speaker Control: 0 enables, 1 disables */
    IDE_RESET_ENABLE = 1 << 7   /* Reset IDE: 0 does not reset, 1 resets */
} psg_port_a_control_t;

/* Control bits for PSG Mixer functionality */
typedef enum
{
    CHANNEL_A_TONE = 1 << 0,  /* Tone control for Channel A: 1 disables output */
    CHANNEL_B_TONE = 1 << 1,  /* Tone control for Channel B: 1 disables output */
    CHANNEL_C_TONE = 1 << 2,  /* Tone control for Channel C: 1 disables output */
    CHANNEL_A_NOISE = 1 << 3, /* Noise control for Channel A: 1 disables output */
    CHANNEL_B_NOISE = 1 << 4, /* Noise control for Channel B: 1 disables output */
    CHANNEL_C_NOISE = 1 << 5, /* Noise control for Channel C: 1 disables output */
    PORT_A_IN_OUT = 1 << 6,   /* PSG Port A Input/Output control: 1 sets as output */
    PORT_B_IN_OUT = 1 << 7    /* PSG Port B Input/Output control: 1 sets as output */
} psg_mixer_control_t;

/* Externally declared pointers to PSG registers */
extern IO_PORT8_RO psg_reg_read; /* PSG register for read operations */
extern IO_PORT8 psg_reg_select;  /* PSG register select */
extern IO_PORT8 psg_reg_write;   /* PSG register write */

/* Status bits for WD1772 Floppy Disk Controller (FDC) functionality */
typedef enum
{
    FDC_BUSY = 1 << 0,                /* FDC is busy with a command */
    FDC_INDEX_DATA_REQUEST = 1 << 1,  /* FDC has raised an index/data request */
    FDC_TRACK_ZERO = 1 << 2,          /* FDC's read/write head is at track zero */
    FDC_LOST_DATA = 1 << 2,           /* Data has been lost due to slow CPU processing */
    FDC_CRC_ERROR = 1 << 3,           /* Data CRC error has occurred */
    FDC_RECORD_NOT_FOUND = 1 << 4,    /* Requested track, sector, or side not found */
    FDC_SPIN_UP_RECORD_TYPE = 1 << 5, /* Spin-up complete or specific record type indicated */
    FDC_WRITE_PROTECT = 1 << 6,       /* Disk is write protected */
    FDC_MOTOR_ON = 1 << 7             /* Drive motor is powered on */
} wdc_fdc_access_t;

/* Macros for checking errors based on FDC status flags */
#define FDC_RESTORE_ERROR_CHECK(fdc_access) (!((fdc_access) & FDC_TRACK_ZERO))
#define FDC_SEEK_ERROR_CHECK(fdc_access) ((fdc_access) & FDC_RECORD_NOT_FOUND)
#define FDC_WRITE_ERROR_CHECK(fdc_access) ((fdc_access) & (FDC_WRITE_PROTECT | FDC_LOST_DATA | FDC_RECORD_NOT_FOUND))
#define FDC_READ_ERROR_CHECK(fdc_access) ((fdc_access) & (FDC_CRC_ERROR | FDC_LOST_DATA | FDC_RECORD_NOT_FOUND))

/* Status bits for DMA operations related to the FDC */
typedef enum
{
    DMA_OK_ERROR_STATUS = 1 << 0,   /* DMA operation completed successfully */
    DMA_SECTOR_COUNT_NOT0 = 1 << 1, /* DMA sector count is not zero */
    DMA_DATREQ_CONDITION = 1 << 2   /* State of FDC DATA REQUEST signal */
} wdc_dma_status_t;

/* Mode control bits for the DMA (Direct Memory Access) */
typedef enum
{
    DMA_NOT_NEWCMDBLK = 1 << 1,           /* Indicates if command block is new or not */
    DMA_A0_CONTROL = 1 << 1,              /* Controls the A0 line, often same as DMA_NOT_NEWCMDBLK */
    DMA_A1_CONTROL = 1 << 2,              /* Controls the A1 line */
    DMA_CHIP_SELECT = 1 << 3,             /* Selects chip for DMA */
    DMA_SECTOR_COUNT_REG_SELECT = 1 << 4, /* Selects between DMA internal sector count or HDC/FDC */
    TRANSFER_DIRECTION = 1 << 7,          /* Sets the direction of data transfer */
} wdc_dma_mode_t;

/* Externally declared pointers to FDC and DMA registers */
extern IO_PORT16 fdc_access;    /* Access register for the FDC */
extern IO_PORT16 dma_mode;      /* Mode control register for DMA */
extern IO_PORT16_RO dma_status; /* Status register for DMA, read-only */

/* Externally declared pointers for DMA base address setting */
extern IO_PORT8 WDC_DMA_BASE_HIGH; /* High byte of the DMA base address */
extern IO_PORT8 WDC_DMA_BASE_MID;  /* Middle byte of the DMA base address */
extern IO_PORT8 WDC_DMA_BASE_LOW;  /* Low byte of the DMA base address */

/* Macros for setting the DMA base address */
#define SET_DMA_ADDRESS_HIGH_BYTE(val) (*WDC_DMA_BASE_HIGH = (val))
#define SET_DMA_ADDRESS_MID_BYTE(val) (*WDC_DMA_BASE_MID = (val))
#define SET_DMA_ADDRESS_LOW_BYTE(val) (*WDC_DMA_BASE_LOW = (val))

/* Macro for setting the entire DMA base address */
#define SET_DMA_ADDRESS(address)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        SET_DMA_ADDRESS_HIGH_BYTE((char)((long)address >> 16) & 0xFF);                                                 \
        SET_DMA_ADDRESS_MID_BYTE((char)((long)address >> 8) & 0xFF);                                                   \
        SET_DMA_ADDRESS_LOW_BYTE((char)address & 0xFF);                                                                \
    } while (0)

/* Define DMA mode read and write values */
#define DMA_MODE_READ 0x00  /* DMA mode for read operations */
#define DMA_MODE_WRITE 0x01 /* DMA mode for write operations */

/* Define DMA read mode command register address */
#define DMA_COMMAND_REG_READ (DMA_MODE_READ << 8 | 0x80)

/* Define DMA read mode track registry address */
#define DMA_TRACK_REG_READ (DMA_MODE_READ << 8 | 0x82)

/* Define DMA read mode sector registry address */
#define DMA_SECTOR_REG_READ (DMA_MODE_READ << 8 | 0x84)

/* Define DMA read mode data registry address */
#define DMA_DATA_REG_READ (DMA_MODE_READ << 8 | 0x86)

/* Define DMA write mode command register address */
#define DMA_COMMAND_REG_WRITE (DMA_MODE_WRITE << 8 | 0x80)

/* Define DMA write mode track registry address */
#define DMA_TRACK_REG_WRITE (DMA_MODE_WRITE << 8 | 0x82)

/* Define DMA write mode sector registry address */
#define DMA_SECTOR_REG_WRITE (DMA_MODE_WRITE << 8 | 0x84)

/* Define DMA write mode data registry address */
#define DMA_DATA_REG_WRITE (DMA_MODE_WRITE << 8 | 0x86)

/* Define DMA write mode sector count registry address */
#define DMA_COUNT_REG_WRITE (DMA_MODE_WRITE << 8 | 0x90)

/* Define FDC command bytes */
#define FDC_CMD_RESTORE 0x00   /* restore the FDC's position to track 0 */
#define FDC_CMD_SEEK 0x10      /* seek the FDC to a specific track */
#define FDC_CMD_STEP 0x20      /* step the FDC in the current direction */
#define FDC_CMD_STEPI 0x40     /* step the FDC inwards */
#define FDC_CMD_STEPO 0x60     /* step the FDC outwards */
#define FDC_CMD_READ 0x80      /* read a sector */
#define FDC_CMD_WRITE 0xA0     /* write a sector */
#define FDC_CMD_READID 0xC0    /* read the sector ID */
#define FDC_CMD_READTR 0xE0    /* read an entire track */
#define FDC_CMD_WRITETR 0xF0   /* write an entire track */
#define FDC_CMD_INTERRUPT 0xD0 /* force an interrupt */

/* Define FDC command bits */
#define BIT_U_UPDATE_TRACK_REG (1 << 4)
#define BIT_H_DISABLE_SPIN_UP (1 << 3)
#define BIT_V_VERIFY_ON_DEST_TRACK (1 << 2)
#define BIT_P_DISABLE_WRITE_PRECOMP (1 << 1)
#define BIT_A0_WRITE_NORMAL_DATA_MARK (0 << 0)
#define BIT_A0_WRITE_DELETED_DATA_MARK (1 << 0)

/* Define FDC stepping rate bits */
#define BIT_RL_R0_6MS (0 << 1 | 0 << 0)  /* 6ms stepping rate */
#define BIT_RL_R0_12MS (0 << 1 | 1 << 0) /* 12ms stepping rate */
#define BIT_RL_R0_2MS (1 << 1 | 0 << 0)  /* 2ms stepping rate */
#define BIT_RL_R0_3MS (1 << 1 | 1 << 0)  /* 3ms stepping rate */

/* Define FDC interrupt condition bits */
#define BIT_INT_CONDITION_IMMEDIATE (1 << 3 | 0 << 2 | 0 << 1 | 0 << 0)      /* Immediate interrupt condition */
#define BIT_INT_CONDITION_ON_INDEX_PULSE (0 << 3 | 1 << 2 | 0 << 1 | 0 << 0) /* Interrupt condition on index pulse */
#define BIT_INT_CONDITION_TERMINATE (0 << 3 | 0 << 2 | 0 << 1 | 0 << 0)      /* Terminate condition without interrupt */

/* Define FDC multiple sector flag bit */
#define BIT_M_MULTIPLE_SECTOR (1 << 4) /* Multiple sector operation flag bit */

/* Define additional settling delay for FDC */
#define BIT_E_ADD_15MS_DELAY (1 << 2) /* Additional 15ms delay flag bit */

/* Define OLD MACROS  and use note for replacement */
#define FDC_FLAG_STEP_RATE_6 0x00
#define FDC_FLAG_STEP_RATE_12 0x01
#define FDC_FLAG_STEP_RATE_2 0x02
#define FDC_FLAG_STEP_RATE_3 0x03
#define FDC_FLAG_V_VERIFY_SECTOR_ID 0x04
#define FDC_FLAG_SUPPRESS_MOTOR_ON 1 << 3
#define FDC_FLAG_UPDATE_TRACK_REG 0x10
#define FDC_FLAG_SETTLE_DELAY 0x04
#define FDC_FLAG_MULTI_SECTOR 0x10
#define FDC_FLAG_WRITE_PRECOMPENSATION 0x02
#define FDC_FLAG_SUPPRESS_DATA_ADDR_MARK 0x01
#define FDC_FLAG_INTERRUPT_INDEX_PULSE 0x04
#define FDC_FLAG_FORCE_INTERRUPT 0x08

extern const UINT8 restore_command;
extern const UINT8 seek_command;
extern const UINT8 read_command;
extern const UINT8 write_command;
extern const UINT8 write_deleted_data_command;

/* Define the double density stepping rates */
#define STEPRATE_6MS 0
#define STEPRATE_12MS 1
#define STEPRATE_2MS 2
#define STEPRATE_3MS 3

/* Define delay constants for 68000 processor at different clock speeds */
#define DELAY_68000_16MHZ 760
#define DELAY_68000_8MHZ 320

/* Define a rough guess for a delay time */
#define DELAY_GUESS (DELAY_68000_8MHZ / 10)

/* Define DMA sector count high and low bytes */
#define DMA_SECTOR_COUNT_HIGH (IO_PORT8)0xFFFF8609
#define DMA_SECTOR_COUNT_LOW (IO_PORT8)0xFFFF860B
#define SECTOR_LENGTH 1

/* Struct definition for a disk I/O request */
typedef struct
{
    disk_operation_t operation; /* Operation type, READ or WRITE */
    disk_selection_t disk;      /* Disk selection, DRIVE_A or DRIVE_B */
    disk_side_t side;           /* Disk side, SIDE_0 or SIDE_1 */
    int track;                  /* Track number involved in the operation */
    int sector;                 /* Sector number involved in the operation */
    void *buffer_address;       /* Pointer to data buffer for R/W operations */
    int n_sector;               /* Number of sectors for the operation (NOT IMPLEMENTED) */
} disk_io_request_t;

/* Initializes the floppy drive by setting up the FDC and DMA for disk operations */
int initialize_floppy_driver(void);

/* Interrupt handler for the floppy drive. To be called when an FDC interrupt occurs */
void handle_floppy_interrupt(void);

/* Configures the DMA buffer address to be used for read/write operations */
void setup_dma_buffer(void *buffer_address);

/* Prepares the DMA for read/write operations for a specified disk, side, and track */
int setup_dma_for_rw(disk_selection_t disk, disk_side_t side, int track);

/* Selects the floppy drive and its side for operations */
void select_floppy_drive(disk_selection_t drive, disk_side_t side);

/* Sends a command byte to the FDC */
void send_command_to_fdc(UINT8 command);

/* Waits while the FDC is busy handling the previous command */
void busy_wait(void);

/* Retrieves the current track number from the FDC */
char get_fdc_track(void);

/* Sets the target sector number for the next read/write operation */
void set_fdc_sector(int sector);

/* Sets the target track number for the FDC */
void set_fdc_track(int track);

/* Issues the restore command to the FDC, resetting it to track 0 */
int do_fdc_restore_command(void);

/* Issues a seek command to move the FDC head to a specified track */
int do_fdc_seek_command(void);

/* Initiates a read operation from a disk sector into the specified buffer */
int do_fdc_read_command(void);

/* Initiates a write operation to a disk sector from the specified buffer */
int do_fdc_write_command(void);

/* Moves the FDC head to the specified track */
int seek(int track);

/* Reads a sector into a buffer */
int read_sector(int sector);

/* Writes data from a buffer to a sector */
int write_sector(int sector);

/* Handles a read operation from the floppy, as specified by the I/O request */
int perform_read_operation_from_floppy(disk_io_request_t *io);

/* Handles a write operation to the floppy, as specified by the I/O request */
int perform_write_operation_to_floppy(disk_io_request_t *io);

/* Executes a disk I/O operation as specified by the disk I/O request structure */
int do_disk_operation(disk_io_request_t *disk_io_req);

#endif /* FDC_H*/
