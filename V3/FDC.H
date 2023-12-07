#ifndef FDC_H
#define FDC_H

#include "TYPES.H"

#define CB_SECTOR 512
#define MAX_TRACK 40
#define MAX_SECTOR 9

#define FLOPPY_MOTOR_TIMEOUT 1000000 /* Timeout for motor spin-up */
#define FDC_TIMEOUT 15000 /* timeout for FDC */

typedef enum
{
    BUSY,
    COMMAND_COMPLETE,
    LOST_DATA,
    CRC_ERROR,
    RECORD_NOT_FOUND,
    WRITE_PROTECT,
    SEEK_DONE,
    BUSY_ZERO,
    WRITE_DONE,
    READ_DONE
} disk_status_t;

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

extern IO_PORT8_RO psg_reg_read;
extern IO_PORT8 psg_reg_select;
extern IO_PORT8 psg_reg_write;

typedef enum
{
    /* Bit 0 - Busy: 1 when the 177x is busy, 0 when it is free for CPU commands */
    FDC_BUSY = 1 << 0,
    /* Bit 1 - Index / Data Request: High during index pulse for Type I commands, signals CPU for data handling in Type
     * II and III commands */
    FDC_INDEX_DATA_REQUEST = 1 << 1,
    /* Bit 2 - Track Zero /  Indicates track zero position after Type I commands */
    FDC_TRACK_ZERO = 1 << 2,
    /* Bit 2 - Lost data lost data due to slow CPU for Type II and III commands */
    FDC_LOST_DATA = 1 << 2,
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

#define FDC_RESTORE_ERROR_CHECK(fdc_access) (!((fdc_access) & FDC_TRACK_ZERO))
#define FDC_SEEK_ERROR_CHECK(fdc_access) ((fdc_access) & FDC_RECORD_NOT_FOUND)
#define FDC_WRITE_ERROR_CHECK(fdc_access) ((fdc_access) & (FDC_WRITE_PROTECT | FDC_LOST_DATA | FDC_RECORD_NOT_FOUND))
#define FDC_READ_ERROR_CHECK(fdc_access) ((fdc_access) & (FDC_CRC_ERROR | FDC_LOST_DATA | FDC_RECORD_NOT_FOUND))

typedef enum
{
    /* 1=ok, 0=err */
    DMA_OK_ERROR_STATUS = 1 << 0,
    /* 1=sector count not zero */
    DMA_SECTOR_COUNT_NOT0 = 1 << 1,
    /* 1=Condition of FDC DATA REQUEST signal */
    DMA_DATREQ_CONDITION = 1 << 2
} wdc_dma_status_t;

typedef enum
{
    /* Bit 0 - Unused, must be set to 0 */
    /* Bit 1 - Controls CA1 output (FDC A0 line and HDC command block start) */
    DMA_NOT_NEWCMDBLK = 1 << 1,
    DMA_A0_CONTROL = 1 << 1,
    /* Bit 2 - Controls CA2 output (FDC A1 line) */
    DMA_A1_CONTROL = 1 << 2,
    /* Bit 3 - Selects HDCS* or FDCS* chip-select (1: HDCS*, 0: FDCS*) */
    DMA_CHIP_SELECT = 1 << 3,
    /* Bit 4 - Select DMA internal sect cnt or HDC/FDC external */
    /* controller registers (1: internal, 0: external) */
    DMA_SECTOR_COUNT_REG_SELECT = 1 << 4,
    /* Bit 5 - Reserved, must be set to 0 */
    /* Bit 6 - Unused, set to 0 (historically for DMA enable/disable) */
    /* Bit 7 - Sets data transfer direction  */
    /* (1: write to controller, 0: read from controller) */
    TRANSFER_DIRECTION = 1 << 7,
} wdc_dma_mode_t;

extern IO_PORT16 fdc_access;
extern IO_PORT16 dma_mode;
extern IO_PORT16_RO dma_status;

extern IO_PORT8 WDC_DMA_BASE_HIGH;
extern IO_PORT8 WDC_DMA_BASE_MID;
extern IO_PORT8 WDC_DMA_BASE_LOW;

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

/* commands */

/* [hvrr] restore seeks to track 0  */
#define FDC_CMD_RESTORE 0x00
/* [hvrr] seek to track  */
#define FDC_CMD_SEEK 0x10
/* [uhvrr] step to same direction  */
#define FDC_CMD_STEP 0x20
/* [uhvrr] step in */
#define FDC_CMD_STEPI 0x40
/* [uhvrr] step out */
#define FDC_CMD_STEPO 0x60
/* [mhe00] read sector */
#define FDC_CMD_READ 0x80
/* [mhepa] write sector  */
#define FDC_CMD_WRITE 0xA0
/* [he00] read sector ID */
#define FDC_CMD_READID 0xC0
/* [he00] read track */
#define FDC_CMD_READTR 0xE0
/* [hep0] write track */
#define FDC_CMD_WRITETR 0xF0
/* force interrupt */
#define FDC_CMD_INTERRUPT 0xD0

/* bits for commands */

/* Update Flag [BIT 4] 0: No Update 1: Update Track Register */
#define BIT_U_UPDATE_TRACK_REG (1 << 4)

/* Motor On Flag [BIT 3] 0: Enable Spin-up Sequence 1: Disable Spin-up Sequence */
#define BIT_H_DISABLE_SPIN_UP (1 << 3)

/* Verify Flag [BIT 2] 0: No Verify 1: Verify on Destination Track */
#define BIT_V_VERIFY_ON_DEST_TRACK (1 << 2)

/* Write Precompensation [BIT 1] 0: Unable Write Pre-comp 1: Disable Write Pre-comp */
#define BIT_P_DISABLE_WRITE_PRECOMP (1 << 1)

/* Data Address Mark [BIT 0] 0: Write Normal Data Mark 1: Write Deleted Data Mark */
#define BIT_A0_WRITE_NORMAL_DATA_MARK (0 << 0)
#define BIT_A0_WRITE_DELETED_DATA_MARK (1 << 0)

/* Stepping Rate [Bits 1,0] */
/* 00: 6 ms, 01: 12 ms, 10: 2 ms, 11: 3 ms */
#define BIT_RL_R0_6MS (0 << 1 | 0 << 0)
#define BIT_RL_R0_12MS (0 << 1 | 1 << 0)
#define BIT_RL_R0_2MS (1 << 1 | 0 << 0)
#define BIT_RL_R0_3MS (1 << 1 | 1 << 0)

/* Interrupt Condition [Bits 3-0] */
/* 1000: Immediate Interrupt, 0100: Interrupt on Index Pulse, 0000: Terminate without interrupt */
#define BIT_INT_CONDITION_IMMEDIATE (1 << 3 | 0 << 2 | 0 << 1 | 0 << 0)
#define BIT_INT_CONDITION_ON_INDEX_PULSE (0 << 3 | 1 << 2 | 0 << 1 | 0 << 0)
#define BIT_INT_CONDITION_TERMINATE (0 << 3 | 0 << 2 | 0 << 1 | 0 << 0)

/* Multiple Sector Flag [Bit 4] */
#define BIT_M_MULTIPLE_SECTOR (1 << 4)

/* Settling Delay [Bit 2] */
#define BIT_E_ADD_15MS_DELAY (1 << 2)

/* OLD MACROS [TODO] CHANGE THESE TO THE ONES ABOVE */

/* Stepping rate 6 msec, corresponds to value 0x00 */
#define FDC_FLAG_STEP_RATE_6 0x00
/* Stepping rate 12 msec, corresponds to value 0x01 'r0' */
#define FDC_FLAG_STEP_RATE_12 0x01
/* Stepping rate 2 msec, corresponds to value 0x02 'r1' */
#define FDC_FLAG_STEP_RATE_2 0x02
/* Stepping rate 3 msec, corresponds to value 0x03 */
#define FDC_FLAG_STEP_RATE_3 0x03
/* Verify sector ID flag 'V', corresponds to bit 2 */
#define FDC_FLAG_V_VERIFY_SECTOR_ID 0x04
/* Suppress motor on sequence flag 'h', corresponds to bit 3 */
#define FDC_FLAG_SUPPRESS_MOTOR_ON 1 << 3
/* Update track register flag 'u', corresponds to bit 4 */
#define FDC_FLAG_UPDATE_TRACK_REG 0x10
/* Wait 30 msec to settle flag 'e', corresponds to bit 2 */
#define FDC_FLAG_SETTLE_DELAY 0x04
/* Multi-sector flag 'm', corresponds to bit 4 */
#define FDC_FLAG_MULTI_SECTOR 0x10
/* Write precompensation flag 'p', corresponds to bit 1 */
#define FDC_FLAG_WRITE_PRECOMPENSATION 0x02
/* Suppress data address mark flag 'a0', corresponds to bit 0 */
#define FDC_FLAG_SUPPRESS_DATA_ADDR_MARK 0x01
/* Interrupt on each index pulse flag, corresponds to bit 2 */
#define FDC_FLAG_INTERRUPT_INDEX_PULSE 0x04
/* Force interrupt flag, corresponds to bit 3 */
#define FDC_FLAG_FORCE_INTERRUPT 0x08

extern const UINT8 restore_command;
extern const UINT8 seek_command;
extern const UINT8 read_command;
extern const UINT8 write_command;
extern const UINT8 write_deleted_data_command;

/*
    NOTE: Typically if you wanted to simulate the behavior where the motor spin-up sequence is skipped
            (assuming the motor is already running), then you would include the `h` flag.
            This is often the case when you have issued a command recently and you know the motor has not had time
            to spin down.
*/
/* FDC_FLAG_R1 => 2000 cycles */

/* double density stepping rates */
#define STEPRATE_6MS 0
#define STEPRATE_12MS 1
#define STEPRATE_2MS 2
#define STEPRATE_3MS 3

/* 1 millisecond delay */

/* 68000 timing assumes 16MHz */
#define DELAY_68000_16MHZ 760

/* 68000 timing assumes 8MHz */
#define DELAY_68000_8MHZ 320

#define DELAY_GUESS (DELAY_68000_8MHZ / 10)

typedef struct
{ /* Type of operation (e.g., READ, WRITE) */
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
    volatile UINT8 *buffer_address;
    /* Number of sectors to read/write (NOT IMPLEMENTED)*/
    int n_sector;
} disk_io_request_t;

int initialize_floppy_driver(void);
void handle_floppy_interrupt(void);
void setup_dma_buffer(void *buffer_address);
void setup_dma_for_rw(disk_selection_t disk, disk_side_t side, int track);

void select_floppy_drive(disk_selection_t drive, disk_side_t side);
void busy_wait_with_timeout(UINT32 timeout);
void send_command_to_fdc(UINT8 command);
void busy_wait(void);

char get_fdc_track(void);
void set_fdc_sector(int sector);
void set_fdc_track(int track);

void do_fdc_restore_command(void);
void do_fdc_seek_command(void);
void do_fdc_read_command(void);
void do_fdc_write_command(void);

void seek(int track);
void read_sector(int sector);
void write_sector(int sector);

void perform_read_operation_from_floppy(disk_io_request_t *io);
void perform_write_operation_to_floppy(disk_io_request_t *io);

void do_disk_operation(disk_io_request_t *disk_io_req);
void init_floppy_flags(void);

extern int disk_operation(disk_io_request_t *disk_io_req);
extern void sys_disk_operation(void);

extern int waitfstatus(void);
extern void sys_waitfstatus(void);

#endif /* FDC_H*/
