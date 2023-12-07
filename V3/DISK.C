typedef unsigned char UINT8;
typedef unsigned int UINT16;
typedef unsigned long UINT32;

typedef void (*Vector)();

typedef volatile UINT8 *const IO_PORT8;
typedef volatile const UINT8 *const IO_PORT8_RO;
typedef volatile UINT16 *const IO_PORT16;
typedef volatile const UINT16 *const IO_PORT16_RO;

/* FDC Registers Access / DMA Sector Count Register */
/* functions as both the FDC Registers Access and the DMA Sector Count Register */
/* Its operation is determined by the status of bit 4 in the DMA mode register */
IO_PORT16 WDC_SECTOR_COUNT = (IO_PORT16)0xFFFF8604;

/* DMA Status word; 16-bit register used to read the DMA Status word */
IO_PORT16_RO DMA_STATUS_REGISTER = (IO_PORT16_RO)0xFFFF8606;

/* DMA Mode Control register; 16-bit register used for controlling the DMA Mode */
IO_PORT16 DMA_CONTROL_REGISTER = (IO_PORT16)0xFFFF8606;

typedef union {
    struct
    {
        UINT16 DMA_ERROR_STATUS : 1;       /* Bit 0 - Indicates DMA error (1: error, 0: no error) */
        UINT16 SECTOR_COUNT_STATUS : 1;    /* Bit 1 - Sector count status (1: not zero, 0: zero) */
        UINT16 DATA_REQUEST_CONDITION : 1; /* Bit 2 - Data request condition (1: data request, 0: no request) */
        UINT16 RESERVED_STATUS : 13;       /* Bits 3-15 - Reserved bits for status register */
    } STATUS;                              /* Reading from DMA_STATUS_REGISTER */
    struct
    {
        UINT16 UNUSED : 1;                  /* Bit 0 - Unused, must be set to 0 */
        UINT16 CA1_CONTROL : 1;             /* Bit 1 - Controls CA1 output (FDC A0 line and HDC command block start) */
        UINT16 CA2_CONTROL : 1;             /* Bit 2 - Controls CA2 output (FDC A1 line) */
        UINT16 HDCS_FDCS_SELECT : 1;        /* Bit 3 - Selects HDCS* or FDCS* chip-select (1: HDCS*, 0: FDCS*) */
        UINT16 SECTOR_COUNT_REG_SELECT : 1; /* Bit 4 - Selects DMA internal sector count register or HDC/FDC external */
        /*         controller registers (1: internal, 0: external) */
        UINT16 RESERVED_MODE_5 : 1;         /* Bit 5 - Reserved, must be set to 0 */
        UINT16 DMA_UNUSED : 1;              /* Bit 6 - Unused, set to 0 (historically for DMA enable/disable) */
        UINT16 TRANSFER_DIRECTION : 1;      /* Bit 7 - Sets data transfer direction  */
        /*         (1: write to controller, 0: read from controller) */
        UINT16 RESERVED_MODE : 8;           /* Bits 8-15 - Reserved bits for mode register */
    } MODE;                                 /* Writing to DMA_CONTROL_REGISTER */
} WDC_DMA;
#define WDC_DMA_ADDRESS ((volatile WDC_DMA *)0xFFFF8606)
#define DMA_STATUS WDC_DMA_ADDRESS->STATUS
#define DMA_MODE WDC_DMA_ADDRESS->MODE

/* DMA Status Register bits */
#define DMA_ERROR_BIT (1 << 0)
#define DMA_SC_NONZERO_BIT (1 << 1)
#define DMA_DRQ_BIT (1 << 2)

/* DMA Mode Register bits */
#define DMA_MODE_READ_BIT (0 << 8)
#define DMA_MODE_WRITE_BIT (1 << 8)
#define DMA_SC_REG_ACCESS_BIT (1 << 4)
#define DMA_HDCS_SELECT_BIT (1 << 3)
#define DMA_READ_WRITE_BITS (3 << 8) // Combined bit mask for read/write mode bits

/* DMA Mode Control */
#define SET_DMA_READ_MODE() (*DMA_MODE_REGISTER = DMA_MODE_READ_BIT)
#define SET_DMA_WRITE_MODE() (*DMA_MODE_REGISTER = DMA_MODE_WRITE_BIT)
#define ENABLE_DMA_SC_REG() (*DMA_MODE_REGISTER |= DMA_SC_REG_ACCESS_BIT)
#define DISABLE_DMA_SC_REG() (*DMA_MODE_REGISTER &= ~DMA_SC_REG_ACCESS_BIT)

/* Access DMA Sector Count Register */
#define SET_DMA_SECTOR_COUNT(count) (*WDC_SECTOR_COUNT = count)

/* read & write from to the FDC registers through DMA */
#define READ_FDC_REGISTER(reg) (DMA_STATUS_REGISTER + reg)
#define WRITE_FDC_REGISTER(reg, val) (*(DMA_CONTROL_REGISTER + reg) = val)

/* DMA Direction Control */
#define DMA_DIRECTION_WRITE 0x2 /* D2 bit set for write */
#define DMA_DIRECTION_READ 0x0  /* D2 bit clear for read */

/* DMA Mode Register Operations : FDC Register Mapping in DMA Mode */
#define DMA_MODE_READ 0x00
#define DMA_MODE_WRITE 0x01

/* DMA Register Addresses for Read Mode */
#define DMA_FDC_REG_CONTROL_STATUS_READ (DMA_MODE_READ << 8 | 0x80)
#define DMA_FDC_TRACK_REG_READ (DMA_MODE_READ << 8 | 0x82)
#define DMA_FDC_SECTOR_REG_READ (DMA_MODE_READ << 8 | 0x84)
#define DMA_FDC_DATA_REG_READ (DMA_MODE_READ << 8 | 0x86)
#define DMA_COUNT_REG_READ (DMA_MODE_READ << 8 | 0x90) /* Note: This register is write-only */

/* DMA Register Addresses for Write Mode */
#define DMA_FDC_REG_CONTROL_STATUS_WRITE (DMA_MODE_WRITE << 8 | 0x80)
#define DMA_FDC_TRACK_REG_WRITE (DMA_MODE_WRITE << 8 | 0x82)
#define DMA_FDC_SECTOR_REG_WRITE (DMA_MODE_WRITE << 8 | 0x84)
#define DMA_FDC_DATA_REG_WRITE (DMA_MODE_WRITE << 8 | 0x86)
#define DMA_COUNT_REG_WRITE (DMA_MODE_WRITE << 8 | 0x90)

/* Setting DMA mode for reading & writing from the FDC */
#define SET_DMA_MODE_READ() (*DMA_CONTROL_REGISTER = (DMA_FDC_REG_CONTROL_STATUS_READ))
#define SET_DMA_MODE_WRITE() (*DMA_CONTROL_REGISTER = (DMA_FDC_REG_CONTROL_STATUS_WRITE))

/* Note: The DMA count register (0x90) is write-only. Ensure that it is not read from. */

/* DMA Mode - Accessing Sector Count Register */
#define DMA_MODE_SECTOR_COUNT 0x0100

/* DMA Mode - Accessing FDC Registers for Data Transfer */
#define DMA_FDC_TRANSFER_READ 0x0080  /* Read from FDC */
#define DMA_FDC_TRANSFER_WRITE 0x0180 /* Write to FDC */

/* FDC Command Base Macros */
#define FDC_CMD_RESTORE 0x00     /* Restore command base */
#define FDC_CMD_SEEK 0x10        /* Seek command base */
#define FDC_CMD_READ 0x80        /* Read Sector command base */
#define FDC_CMD_WRITE 0xA0       /* Write Sector command base */
#define FDC_CMD_READ_ADDR 0xC0   /* Read Address command base */
#define FDC_CMD_READ_TRACK 0xE0  /* Read Track command base */
#define FDC_CMD_WRITE_TRACK 0xF0 /* Write Track command base */
#define FDC_CMD_FORCE_INT 0xD0   /* Force Interrupt command base */

/* FDC Command Modifier Flags */
#define FDC_FLAG_MOTOR_ON 0x04       /* Motor On flag 'h' */
#define FDC_FLAG_VERIFY 0x02         /* Verify flag 'v' */
#define FDC_FLAG_STEP_RATE 0x03      /* Stepping Rate 'r1r0' flags (mask of 0x03 for the two least significant bits) */
#define FDC_FLAG_MULTIPLE_SECT 0x10  /* Multiple Sector flag 'm' */
#define FDC_FLAG_DELAY 0x04          /* Delay flag 'e' */
#define FDC_FLAG_WRITE_PRECOMP 0x02  /* Write Precompensation 'p' */
#define FDC_FLAG_DATA_ADDR_MARK 0x01 /* Data Address Mark 'a0' */
#define FDC_FLAG_INT_CONDITION                                                                                         \
0x0F /* Interrupt 'i3i2i1i0' flags (mask of 0x0F for the four least significant bits)                              \
    */

/* DMA Transfer Macros */
#define FDC_CMD_FORCE_INTERRUPT_WITH_ALL_CONDITIONS (FDC_CMD_FORCE_INT | FDC_FLAG_INT_CONDITION)
#define SET_DMA_MODE_TO_TRANSFER_SECTOR_COUNT() (*DMA_CONTROL_REGISTER = DMA_MODE_SECTOR_COUNT)
#define START_DMA_READ_TRANSFER() (*DMA_CONTROL_REGISTER = DMA_FDC_TRANSFER_READ)
#define START_DMA_WRITE_TRANSFER() (*DMA_CONTROL_REGISTER = DMA_FDC_TRANSFER_WRITE)

/* DMA Address Counter ; used to define the 24-bit internal address for the DMA operation */
IO_PORT8 WDC_DMA_BASE_HIGH = (IO_PORT8)0xFFFF8609; /* DMA base address high byte */
IO_PORT8 WDC_DMA_BASE_MID = (IO_PORT8)0xFFFF860B;  /* DMA base address middle byte */
IO_PORT8 WDC_DMA_BASE_LOW = (IO_PORT8)0xFFFF860D;  /* DMA base address low byte */

/* DMA Address Setting Macros */
#define SET_DMA_ADDR_HIGH(high_byte) (*WDC_DMA_BASE_HIGH = high_byte)
#define SET_DMA_ADDR_MID(mid_byte) (*WDC_DMA_BASE_MID = mid_byte)
#define SET_DMA_ADDR_LOW(low_byte) (*WDC_DMA_BASE_LOW = low_byte)

#define SET_DMA_ADDRESS_COUNTER(address)                                                                               \
do                                                                                                                 \
{                                                                                                                  \
    SET_DMA_ADDR_HIGH_BYTE((address >> 16) & 0xFF);                                                                \
    SET_DMA_ADDR_MID_BYTE((address >> 8) & 0xFF);                                                                  \
    SET_DMA_ADDR_LOW_BYTE(address & 0xFF);                                                                         \
} while (0)

#define SET_DMA_ADDRESS(high, mid, low)                                                                                \
do                                                                                                                 \
{                                                                                                                  \
    SET_DMA_ADDR_HIGH(high);                                                                                       \
    SET_DMA_ADDR_MID(mid);                                                                                         \
    SET_DMA_ADDR_LOW(low);                                                                                         \
} while (0)

/* Read FDC Register via DMA */
#define READ_FDC_REGISTER(reg_offset) (*(DMA_STATUS_REGISTER + ((reg_offset) >> 1)))

/* Write FDC Register via DMA */
#define WRITE_FDC_REGISTER(reg_offset, val)                                                                            \
do                                                                                                                 \
{                                                                                                                  \
    *(DMA_MODE_REGISTER + ((reg_offset) >> 1)) = (val);                                                            \
} while (0)
