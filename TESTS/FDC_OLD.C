/*
    WARNING: Write precompensation is disabled in this emulation.
        The effect of magnetic interference, which is relevant on high-density
        tracks of real floppy disks, is not replicated in the emulation environment. Write
        precompensation, which would typically adjust the write timing to counteract
        bit-shifting effects, is therefore not necessary and has been deliberately
        disabled.

        In a precise hardware emulation that aims to model the behavior of the floppy disk
        controller accurately, including its handling of track densities and write
        compensation techniques, it might be necessary to enable write precompensation.
*/

/*
    WARNING: The 'm' (Multiple Sectors) bit is not set.
        In the context of emulation, the 'm' bit functionality, which allows for reading
        or writing multiple contiguous sectors in a physical floppy disk drive, does not
        provide the same performance benefits as on actual hardware. This is because an
        emulated environment does not deal with the mechanical constraints of disk
        rotation and head movements. Data access in emulation is already inherently
        sequential, without the need to wait for physical alignment of sectors.

        Therefore, for simplicity and to avoid unnecessary complexity in the emulation
        logic, the 'm' bit is not used. This command will access only a single sector
        at a time.
*/

/*
    WARNING: The 'e' (Delay) flag is not set. In a physical FDC, this flag introduces a delay
    before command execution, typically to allow mechanical components to settle.
*/

/*
        WARNING: The v bit is not set since we only run this in an emulation.
                Also, the r0 bit is 0, and the r1 bit is 1, so we have very short step times.
                If this is ever used on real hardware, you must set the V bit and do proper error checking.
                You will also want to have longer step times. So it would be recommended to set both r1, r0 and V
*/

/*
        WARNING: The FDC_CMD_XXXX commands include the FDC_FLAG_H (Motor On) flag,
                which assumes the floppy drive motor is already running. This flag skips the
                motor spin-up sequence in the emulation, leading to immediate command execution
                without any delay. If you are emulating scenarios where motor spin-up time is
                critical (e.g., timing-sensitive operations or testing motor-related behaviors),
                you should remove the FDC_FLAG_H flag to simulate the actual spin-up delay.
*/

#include "FDC.H"
#include "TYPES.H"

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
            (assuming the motor is already running), then you would include the `h` flag.
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

/* PSG:
    In addition to sound generation, the PSG has I/O ports which can be programmed for various control functions.
        On the Atari ST, one of these I/O ports is used for controlling the drive motors and selecting which floppy
        drive is active as well as which side of the floppy disk is being accessed. Here is a breakdown of what the
        PSG controls in relation to the floppy drives:

    Drive Select: The PSG can select between none, one, or both of the floppy drives (Drive A and Drive B) by setting
   specific bits to high or low. Only one drive should be selected at any given time for a read or write operation to
   ensure the proper functioning of the disk system.

    Side Select: The floppy disks used in the Atari ST are double-sided, and the PSG controls which side of the disk the
   read/write head is working with.

    Motor Control: The PSG can turn the disk drive motor on or off.
        It's typically necessary to ensure the motor is spinning before attempting to read from or write to the disk.
*/

/*
    ##############YM2149 Sound Chip                                    ###########
    -------+-----+-----------------------------------------------------+----------
    $FF8800|byte |Read data/Register select                            |R/W
           |     |0 Channel A Freq Low              BIT 7 6 5 4 3 2 1 0|
           |     |1 Channel A Freq High                     BIT 3 2 1 0|
           |     |2 Channel B Freq Low              BIT 7 6 5 4 3 2 1 0|
           |     |3 Channel B Freq High                     BIT 3 2 1 0|
           |     |4 Channel C Freq Low              BIT 7 6 5 4 3 2 1 0|
           |     |5 Channel C Freq High                     BIT 3 2 1 0|
           |     |6 Noise Freq                          BIT 5 4 3 2 1 0|
           |     |7 Mixer Control                   BIT 7 6 5 4 3 2 1 0|
           |     |  Port B IN/OUT (1=Output) -----------' | | | | | | ||
           |     |  Port A IN/OUT ------------------------' | | | | | ||
           |     |  Channel C Noise (1=Off) ----------------' | | | | ||
           |     |  Channel B Noise --------------------------' | | | ||
           |     |  Channel A Noise ----------------------------' | | ||
           |     |  Channel C Tone (0=On) ------------------------' | ||
           |     |  Channel B Tone ---------------------------------' ||
           |     |  Channel A Tone -----------------------------------'|
           |     |8 Channel A Amplitude Control           BIT 4 3 2 1 0|
           |     |  Fixed/Variable Level (0=Fixed) -----------' | | | ||
           |     |  Amplitude level control --------------------+-+-+-'|
           |     |9 Channel B Amplitude Control           BIT 4 3 2 1 0|
           |     |  Fixed/Variable Level ---------------------' | | | ||
           |     |  Amplitude level control --------------------+-+-+-'|
           |     |10 Channel C Amplitude Control          BIT 4 3 2 1 0|
           |     |  Fixed/Variable Level ---------------------' | | | ||
           |     |  Amplitude level control --------------------+-+-+-'|
           |     |11 Envelope Period High           BIT 7 6 5 4 3 2 1 0|
           |     |12 Envelope Period Low            BIT 7 6 5 4 3 2 1 0|
           |     |13 Envelope Shape                         BIT 3 2 1 0|
           |     |  Continue -----------------------------------' | | ||
           |     |  Attack ---------------------------------------' | ||
           |     |  Alternate --------------------------------------' ||
           |     |  Hold ---------------------------------------------'|
           |     |   00xx - \____________________________________      |
           |     |   01xx - /|___________________________________      |
           |     |   1000 - \|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\|\      |
           |     |   1001 - \____________________________________      |
           |     |   1010 - \/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\      |
           |     |   1011 - \|-----------------------------------      |
           |     |   1100 - /|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/      |
           |     |   1101 - /------------------------------------      |
           |     |   1110 - /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/      |
           |     |   1111 - /|___________________________________      |
           |     |14 Port A                         BIT 7 6 5 4 3 2 1 0|
           |     |  IDE Drive On/OFF -------------------+ | | | | | | ||    (F030)
           |     |  SCC A (0=LAN, 1=Serial2) -----------' | | | | | | ||      (TT)
           |     |  Monitor jack GPO pin -----------------+ | | | | | ||
           |     |  Internal Speaker On/Off --------------' | | | | | ||    (F030)
           |     |  Centronics strobe ----------------------' | | | | ||
           |     |  RS-232 DTR output ------------------------' | | | ||
           |     |  RS-232 RTS output --------------------------' | | ||
           |     |  Drive select 1 -------------------------------' | ||
           |     |  Drive select 0 ---------------------------------' ||
           |     |  Drive side select --------------------------------'|
           |     |15 Port B (Parallel port)                            |
    $FF8802|byte |Write data                                           |W
           |     +-----------------------------------------------------+
           |     |Note: PSG Registers are now fixed at these addresses.|
           |     |All other addresses are masked out on the Falcon. Any|
           |     |writes to the shadow registers $8804-$88FF will cause|
           |     |bus errors. Game/Demo coders beware!                 |
    -------+-----+-----------------------------------------------------+----------
*/

void select_floppy_drive(disk_selection_t drive, disk_side_t side)
{
    UINT8 register_state;

    *psg_reg_select = PSG_PORT_A_CONTROL;
    register_state = (*psg_reg_read) & 0xF8;

    /* 0:on,1:off */
    register_state |= drive == DRIVE_A ? DRIVE_B_DISABLE : DRIVE_A_DISABLE;
    register_state |= side == SIDE_SELECT_0;

    *psg_reg_write = register_state;
}

int do_fdc_restore(void)
{
}

int do_fdc_seek(void)
{
}

int do_fdc_read(void *buff)
{
}

int do_fdc_write(const void *buff)
{
}

void set_fdc_track(int track)
{
}

void set_fdc_sector(int sector)
{
}

char get_fdc_track(void)
{
}

int seek(int track)
{
}

int write_sector(int sector, const void *buff)
{
}

int read_sector(int sector, void *buff)
{
}

void busy_wait(void)
{
}

/* Call initialization to set up the driver */
int main(void)
{
    /*
        1. Prepare the Data: Ensure that the data you want to write is stored in memory at a specific location that the
       DMA can access.
        2. Set up the DMA: Program the DMA controller with the address of the data in memory, the direction of the
       transfer (memory-to-disk), the number of bytes to be transferred, and the desired operation mode.
        3. Command the FDC: Before initiating the DMA transfer, you must send commands to the FDC to specify the
       starting track, side, and sector where the writing should begin.
    */

    /*
        The DMA does not automatically increment the sector address. Instead, it is up to your disk driver software to
       control the process. Here's how it generally works:

        1. You send a command to the FDC to write to a specific sector.
        2. You trigger the DMA to start transferring the data for that sector.
        3. Once the DMA and FDC have completed the write operation for that sector,
            the disk driver software must send a new command to the FDC to move to the next sector and then trigger the
       DMA again.
    */

    /*
        for each sector from start_sector to start_sector + (100KB / sector_size):
            set FDC to target sector
            initiate DMA transfer for the sector
            wait for DMA and FDC to signal transfer complete
            increment to next sector and repeat
    */

    return 0;
}
