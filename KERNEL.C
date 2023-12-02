/* IMPORTANT: VERY LIMITED STACK SPACE! AVOID >~5 STACK FRAMES PER SYSTEM CALL
              HANDLER, WITH <=~16 BYTES OF PARAMETERS, <=~16 BYTES OF LOCALS.
              NOTE ALSO THAT ISRs CAN NEST!
*/

#include "FONT.H"
#include "TYPES.H"

/*  system call #:	name:

     0				(reserved)
     1				exit
     2				create_process
     3				write
     4				read
     5				get_pid
     6				(reserved)				(adjust priority?)
     7				yield
     8				wait
     9				(reserved)
    10				semaphore operation
    11				(reserved)
    12				open
    13				close
    14				create_file
    15				delete_file
*/

#define HBL_VECTOR 26
#define VBL_VECTOR 28
#define TRAP_1_VECTOR 33 /* exit           */
#define TRAP_2_VECTOR 34 /* create_process */
#define TRAP_3_VECTOR 35 /* write          */
#define TRAP_4_VECTOR 36 /* read           */
#define TRAP_5_VECTOR 37 /* get_pid        */
#define TRAP_7_VECTOR 39 /* yield          */
#define IKBD_VECTOR 70
#define TIMER_A_VECTOR 77

#define MFP_TIMER_A 0x20
#define MFP_GPIP4 0x40

#define ACIA_OVRN 0x20
#define ACIA_IRQ 0x80

#define get_video_base() ((UINT8 *)0x3F8000)

struct CPU_context /* 70 bytes */
{
    UINT32 pc;
    UINT16 sr;
    UINT32 usp;
    UINT32 d0_to_7_a1_to_6[14];
    UINT32 a0;
};

#define PROC_INVALID 0
#define PROC_READY 1
#define PROC_RUNNING 2
#define PROC_BLOCKED 3

struct process /* 102 bytes */
{
    struct CPU_context cpu_context; /* must be first element in structure */
    UINT16 state;

    UINT16 pid;
    struct process *parent;

    UINT32 dummy1;
    UINT32 dummy2;
    UINT32 dummy3;
    UINT32 dummy4;
    UINT32 dummy5;
    UINT32 dummy6;

    /* [TO DO] add:
            - priority?
            - next/prev pointers for queue insertion?
            - memory bounds?
    */
};

void restart();

void init_IO();
void init_vector_table();
void init_proc_table();

void load(UINT16 i, void (*p)());
void load_cpu_context(struct CPU_context *);

void shell();
void hello();
void user_program_2();
void user_program_3();
void user_program_4();

void exit();
void sys_exit();
void do_exit();
void terminate();

void create_process(UINT16 prog_num, UINT16 is_fg);
void sys_create_process();
void do_create_process(UINT16 prog_num, UINT16 is_fg);

void write(const char *buf, unsigned int len);
void sys_write();
void do_write(const char *buf, unsigned int len);

int read(char *buf, unsigned int len);
void sys_read();
int do_read(char *buf, unsigned int len);

int get_pid();
void sys_get_pid();
int do_get_pid();

void yield();
void sys_yield();
void do_yield();

/* UINT8 *get_video_base(); */
void clear_screen(UINT8 *base);
void plot_glyph(UINT8 ch);
void print_char(char);
void print_str(char *);
void print_char_safe(char);
void print_str_safe(char *);
void scroll();
void invert_cursor();
void reset_cursor();
void clear_cursor();

void vbl_isr();
void do_vbl_isr();
void exception_isr();
void do_exception_isr(UINT16 sr);
void addr_exception_isr();
void do_addr_exception_isr(UINT16 flags, UINT32 addr, UINT16 ir, UINT16 sr);
void timer_A_isr();
void do_timer_A_isr(UINT16 sr);
void ikbd_isr();
void do_ikbd_isr();
void input_enqueue(char ch);

void schedule();
void await_interrupt();

void panic();

UINT16 read_SR();
void write_SR(UINT16 sr);
UINT16 set_ipl(UINT16 ipl);

/* NOTE: only constants may be declared globally! */

IO_PORT8 MFP_IERA = 0xFFFA07;
IO_PORT8 MFP_IERB = 0xFFFA09;
IO_PORT8 MFP_ISRA = 0xFFFA0F;
IO_PORT8 MFP_ISRB = 0xFFFA11;
IO_PORT8 MFP_IMRA = 0xFFFA13;
IO_PORT8 MFP_IMRB = 0xFFFA15;
IO_PORT8 MFP_VR = 0xFFFA17;
IO_PORT8 MFP_TACR = 0xFFFA19;
IO_PORT8 MFP_TADR = 0xFFFA1F;

IO_PORT8 IKBD_ACIA_CR = 0xFFFC00;
IO_PORT8_RO IKBD_ACIA_SR = 0xFFFC00;
IO_PORT8_RO IKBD_ACIA_RDR = 0xFFFC02;

IO_PORT8 MIDI_ACIA_CR = 0xFFFC04;

IO_PORT8 VID_BASE_HI = 0xFF8201;
IO_PORT8 VID_BASE_MID = 0xFF8203;

/* NOTE: kernel data/stack are at addresses 0x000140 <= ... < 0x000800
         since we're only using interrupt vectors 0-79

          600 - 7FF	kernel stack	( 512 bytes)
          140 - 5FF	kernel data		(1216 bytes)		<-- idea: 140-1FF for misc, 200-3FF for process table, 400-4FF
   for console driver, 500-5FF for misc 000 - 13F	vector table
*/

Vector *const vector_table = 0x000000;

UINT8 *const console_x_p = 0x000140; /* [TO DO] group with console driver data */
UINT8 *const console_y_p = 0x000141;
UINT16 *const vbl_counter = 0x000142;
UINT16 *const cursor_visible = 0x000144;

#define MAX_NUM_PROC 4 /* must be a power of 2 */
#define CURR_PROC (proc + *curr_proc)

UINT16 *const curr_proc = 0x000200;
UINT16 *const resched_needed = 0x000202; /* 0=no, 1=yes, 2=yes with eventual trap restart (blocking) */
struct process *const proc = 0x000204;   /* array of MAX_NUM_PROC (4) process structures */

UINT16 *const kybd_isr_state = 0x000400; /* 0=not in mouse packet, 1=expecting delta x, 2=expecting delta y */
UINT16 *const kybd_buff_head = 0x000402;
UINT16 *const kybd_buff_tail = 0x000404;
UINT16 *const kybd_buff_fill = 0x000406;
UINT16 *const kybd_num_lines = 0x000408;
UINT16 *const kybd_len_line = 0x00040A; /* number of characters in buffer for current line */
UINT16 *const kybd_shifted = 0x00040C;
UINT8 *const kybd_auto_ch = 0x00040E;
UINT16 *const kybd_auto_count = 0x000410;
UINT16 *const kybd_blocked_proc = 0x000412;
UINT16 *const kybd_fg_proc = 0x000414;
UINT8 *const kybd_buff = 0x000416; /* 128 byte circular queue - must be a power of 2 */

const UINT8 scan2ascii[2][128] = {
    {                                                    /* unshifted */
     0,   0,   '1', '2', '3',  '4',  '5', '6', '7', '8', /* [TO DO] handle control characters? */
     '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't',  'y', 'u', 'i',  'o', 'p', '[', ']', '\r', 0,
     'a', 's', 'd', 'f', 'g',  'h',  'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v', 'b',  'n',
     'm', ',', '.', '/', 0,    0,    0,   ' ', 0,   0,   0,    0,   0,   0,    0,   0,   0,   0,   0,    0,
     0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,    0,   0,   0,    0,   0,   0,   0,   0,    0,
     0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,    0,   0,   0,    0,   0,   0,   0,   0,    0,
     0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,    0,   0,   0,    0,   0,   0,   0},
    {/* shifted */
     0,   0,   '!', '@', '#', '$', '%',  '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E',  'R', 'T', 'Y',
     'U', 'I', 'O', 'P', '{', '}', '\r', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J',  'K',  'L', ':', '\"', '~', 0,   '|',
     'Z', 'X', 'C', 'V', 'B', 'N', 'M',  '<', '>', '?', 0,   0,   0,   ' ', 0,    0,    0,   0,   0,    0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,    0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,    0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0}};

const Vector prog[] = {shell, hello, user_program_2, user_program_3, user_program_4};

/* FDC */
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

int disk_operation(disk_io_request_t *disk_io_req);
extern void sys_disk_operation(void);
int do_disk_operation(disk_io_request_t *disk_io_req);

int read_operation_from_floppy(disk_io_request_t *io);
int write_operation_to_floppy(disk_io_request_t *io);

/* string helpers */
UINT32 my_strlen(const char *str);

/* added */
void init_console(void);

void init(void)
{
    /* [TO DO] init_memory? */
    init_IO();

    init_vector_table();

    /* [TO DO]
        * fg/bg (deal with parent termination, block instead of terminate when bg process reads)
        - implement wait system call
        - finish console driver
            - clean up code
            - see misc. TO DOs below
        - clean up code incl. assembly; better modularize project
    */

   init_console();

    if (initialize_floppy_driver() == 0)
    {
        return;
    }

    init_proc_table();
    do_create_process(0, 1); /* load shell */

    schedule();
}

void init_console(void)
{
    print_char(CHAR_FF); /* form feed inits console output driver */
    print_str("Welcome to TOY OS (kernel v0.1).\r\n");

    *kybd_isr_state = 0;
    *kybd_buff_head = 1; /* [TO DO] put in init_console function (output too) */
    *kybd_buff_tail = 0;
    *kybd_buff_fill = 0;
    *kybd_num_lines = 0;
    *kybd_len_line = 0;
    *kybd_shifted = 0;
    *kybd_auto_ch = 0;
    *kybd_auto_count = 0;
    *kybd_blocked_proc = -1;
    *kybd_fg_proc = 0;
}


void init_proc_table()
{
    int i;

    *curr_proc = MAX_NUM_PROC - 1;

    for (i = 0; i < MAX_NUM_PROC; i++)
        proc[i].state = PROC_INVALID;
}

void load(UINT16 i, void (*p)())
{
    struct process *c = proc + i;

    c->cpu_context.usp = (UINT32)get_video_base() - ((UINT32)i << 8);
    c->cpu_context.pc = (UINT32)p;
    c->cpu_context.sr = 0x0200;

    c->state = PROC_READY;
    c->pid = i;            /* [TO DO] partially decouple pid from process table index */
    c->parent = CURR_PROC; /* [TO DO] deal with root process? */
}

#define SHELL_BUF_LEN 200

void shell()
{
    char buf[SHELL_BUF_LEN];
    int command;
    UINT32 i;

    while (1)
    {
        write("\r\n$ ", 4);
        read(buf, SHELL_BUF_LEN);

        command = *buf - '0';

        if (command > 0 && command < 5)
        {
            create_process(command, 0);
            /* [TO DO] wait */
        }
    }
}

void do_write(const char *buf, unsigned int len)
{
    /* [TO DO] validate buf */

    for (; len > 0; len--)
        print_char_safe(*(buf++));
}

int do_read(char *buf, unsigned int len)
{
    int num_read = 0, cr_hit = 0;
    UINT16 orig_ipl = set_ipl(6); /* need to mask both MFP IRQ and VBL IRQ */
    /* [TO DO] mask above less aggressively */

    /* [TO DO] validate buf */

    if (*curr_proc == *kybd_fg_proc)
        if (*kybd_num_lines)
            while (len > 0 && !cr_hit)
            {
                if ((buf[num_read++] = kybd_buff[*kybd_buff_head]) == '\r')
                {
                    (*kybd_num_lines)--;
                    cr_hit = 1;
                }

                *kybd_buff_head = (*kybd_buff_head + 1) & 127;
                (*kybd_buff_fill)--;
                len--;
            }
        else
        {
            *kybd_blocked_proc = *curr_proc;
            CURR_PROC->state = PROC_BLOCKED;
            *resched_needed = 2; /* signals the trap will need to be restarted */
                                 /* [TO DO] examine completing system call in bottom half, instead */
        }
    else
    {
        print_str_safe("read attempted by background process ");
        print_char_safe((char)(CURR_PROC->pid + '0'));
        print_str_safe("\r\n");

        terminate();
    }

    set_ipl(orig_ipl);

    return num_read;
}

void do_ikbd_isr()
{
    /* [TO DO] allow unbuffered (raw) mode? */
    /* [TO DO] implement caps lock */
    /* [TO DO] beep if buffer full? */
    /* [TO DO] tweak to minimize chance of overrruns (algorithm, assembly, IRQ prioritization) */

    if (*IKBD_ACIA_SR & ACIA_IRQ)
    {
        UINT8 data = *IKBD_ACIA_RDR;

        switch (*kybd_isr_state)
        {
        case 0:
            if ((data & 0xFC) == 0xF8)
                *kybd_isr_state = 1;
            else if (data == 0x2A || data == 0x36) /* [TO DO] incorporate shifting into autorepeat logic */
                (*kybd_shifted)++;
            else if (data == 0xAA || data == 0xB6)
                (*kybd_shifted)--;
            else if (!(data & 0x80))
            {
                *kybd_auto_ch = scan2ascii[*kybd_shifted ? 1 : 0][data];

                if (*kybd_auto_ch)
                {
                    *kybd_auto_count = 0;
                    input_enqueue(*kybd_auto_ch);
                }
            }
            else
                *kybd_auto_ch = 0;

            break;

        case 1:
            *kybd_isr_state = 2;
            break;

        case 2:
            *kybd_isr_state = 0;
            break;
        }

        /* [TO DO] write a "reset IKBD ACIA" function */

        if (*IKBD_ACIA_SR & ACIA_OVRN) /* overrun during this slow ISR?  IRQ will still be asserted! */
        {
            *IKBD_ACIA_CR = 0x17; /* reset the ACIA to clear IRQ, or the MFP won't re-ack the IRQ ... */
            *IKBD_ACIA_CR = 0x96; /* ... and the IKBD will hang!                                      */
        }
    }

    *MFP_ISRB &= ~MFP_GPIP4;
}

void input_enqueue(char ch)
{
    if (ch == '\b')
    {
        if (*kybd_len_line > 0)
        {
            (*kybd_len_line)--;
            *kybd_buff_tail = (*kybd_buff_tail + 127) & 127;
            (*kybd_buff_fill)--;
            print_char(ch);
        }
    }
    else if (*kybd_buff_fill < 128)
    {
        if (ch == '\r') /* [TO DO] if buffer almost full, a final '\r' should fit */
        {
            (*kybd_num_lines)++;
            *kybd_len_line = 0;

            if (*kybd_blocked_proc != -1)
            {
                proc[*kybd_blocked_proc].state = PROC_READY;
                *kybd_blocked_proc = -1;
            }
        }
        else
            (*kybd_len_line)++;

        *kybd_buff_tail = (*kybd_buff_tail + 1) & 127;
        kybd_buff[*kybd_buff_tail] = ch;
        (*kybd_buff_fill)++;
        print_char(ch);
    }
}

UINT32 my_strlen(const char *str)
{
    const char *s;
    for (s = str; *s; ++s)
    {
        ;
    }
    return s - str;
}

void hello()
{
    char pid = '0' + (char)get_pid();
    char *greeting = "\r\nhello, world from ";

    write(greeting, my_strlen(greeting));
    write(&pid, 1);
    write("!\r\n", 5);
    exit();
}

void user_program_2()
{
    volatile UINT32 i;
    int printed = 0;

    while (printed < 10)
    {
        for (i = 0; i < 30000L; i++)
            ;

        write("B", 1);

        if (++printed == 5)
            create_process(3, 0);
    }

    exit();
}

void user_program_3()
{
    volatile UINT32 i;
    int printed = 0;

    while (printed < 40)
    {
        write("C", 1);
        printed++;

        for (i = 0; i < 10000L; i++)
            ;
    }

    exit();
}

void user_program_4()
{
    volatile UINT32 i;
    int printed = 0;

    while (1)
    {
        for (i = 0; i < 500000L; i++)
            ;

        write("D", 1);

        if (++printed == 10)
            printed / 0;
    }
}

void do_create_process(UINT16 prog_num, UINT16 is_fg)
{
    int i;

    for (i = 0; i < MAX_NUM_PROC && proc[i].state != PROC_INVALID; i++)
        ;

    if (i == MAX_NUM_PROC)
        print_str_safe("process table full\r\n");
    else
    {
        load(i, prog[prog_num]);

        if (*curr_proc == *kybd_fg_proc && is_fg) /* [TO DO] return error condition if false? */
            *kybd_fg_proc = i;
    }
}

UINT16 set_ipl(UINT16 ipl)
{
    UINT16 sr = read_SR();
    UINT16 old_ipl = ((sr >> 8) & 0x0007);

    write_SR((sr & 0xF8FF) | ((ipl & 0x0007) << 8));

    return old_ipl;
}

void init_IO()
{
    /* note: memory & video already configured (in start.s)              */
    /*       ACIAs require software reset                                */
    /*       all other controllers have been hardware reset (in start.s) */

    *MFP_VR = 0x48;

    *MFP_TACR = 0x07; /* delay mode w/ 200 prescaler on 2.4576 MHz */
    *MFP_TADR = 0x00; /* counts down from 256 -> 48 timeouts/s */

    *IKBD_ACIA_CR = 0x17; /* master reset */
    *IKBD_ACIA_CR = 0x96; /* 8N1, Tx IRQs disabled, Rx IRQs enabled, clock divide down by 64 */

    *MIDI_ACIA_CR = 0x57; /* master reset  */
    *MIDI_ACIA_CR = 0x55; /* IRQs disabled (other settings would need to be checked before use */

    *MFP_IERA |= MFP_TIMER_A;
    *MFP_IMRA |= MFP_TIMER_A;

    *MFP_IERB |= MFP_GPIP4;
    *MFP_IMRB |= MFP_GPIP4;
}

void do_timer_A_isr(UINT16 sr)
{
    /* NOTE: timer A is very high priority.  Could use lower (e.g. C) */
    /* NOTE: could lower 68000 IPL to 4 to allow higher priority IRQs */

    if (CURR_PROC->state == PROC_RUNNING)
    {
        CURR_PROC->state = PROC_READY;
        *resched_needed = 1;
    }

    *MFP_ISRA &= ~MFP_TIMER_A;
}

void schedule()
{
    /* NOTE caller must have already saved CPU context */

    int i;

    while (1)
    {
        set_ipl(7);

        for (i = 0; i < MAX_NUM_PROC; i++)
        {
            *curr_proc = (*curr_proc + 1) & (MAX_NUM_PROC - 1);

            if (CURR_PROC->state == PROC_READY)
            {
                CURR_PROC->state = PROC_RUNNING;
                *resched_needed = 0;
                load_cpu_context(&CURR_PROC->cpu_context);
            }
        }

        await_interrupt();
    }
}

void do_exit()
{
    terminate();
}

int do_get_pid()
{
    return CURR_PROC->pid;
}

void do_yield()
{
    CURR_PROC->state = PROC_READY;
    *resched_needed = 1;
}

void init_vector_table()
{
    int num;

    for (num = 2; num < 80; num++)
        vector_table[num] = panic;

    for (num = 2; num <= 3; num++)
        vector_table[num] = addr_exception_isr;

    for (num = 4; num <= 8; num++)
        vector_table[num] = exception_isr;

    for (num = 10; num <= 11; num++)
        vector_table[num] = exception_isr;

    for (num = 32; num <= 47; num++)
        vector_table[num] = exception_isr;

    vector_table[VBL_VECTOR] = vbl_isr;
    vector_table[TRAP_1_VECTOR] = sys_exit;
    vector_table[TRAP_2_VECTOR] = sys_create_process;
    vector_table[TRAP_3_VECTOR] = sys_write;
    vector_table[TRAP_4_VECTOR] = sys_read;
    vector_table[TRAP_5_VECTOR] = sys_get_pid;
    vector_table[TRAP_7_VECTOR] = sys_yield;
    vector_table[IKBD_VECTOR] = ikbd_isr;
    vector_table[TIMER_A_VECTOR] = timer_A_isr;
}

void panic()
{
    volatile UINT32 i;

    set_ipl(7);
    print_str("kernel panic!");

    for (i = 0; i < 100000L; i++)
        ;

    restart();
}

void do_exception_isr(UINT16 sr)
{
    if (sr & 0x2000)
        panic();

    print_str_safe("fault in process ");
    print_char_safe((char)(CURR_PROC->pid + '0'));
    print_str_safe("\r\n");

    terminate();
}

void terminate()
{
    if (*curr_proc == *kybd_fg_proc)
        *kybd_fg_proc = CURR_PROC->parent->pid; /* [TO DO] handle case where parent has terminated! */

    CURR_PROC->state = PROC_INVALID;
    schedule();
}

void do_addr_exception_isr(UINT16 flags, UINT32 addr, UINT16 ir, UINT16 sr)
{
    do_exception_isr(sr);
}

/*
UINT8 *get_video_base()
{
    return (UINT8 *)(
        ((UINT32)*VID_BASE_HI << 16) +
        ((UINT32)*VID_BASE_MID << 8)
    );
}
*/

#define TEXT_OFFSET (get_video_base() + (UINT16) * console_y_p * 640 + *console_x_p)

void plot_glyph(UINT8 ch)
{
    UINT8 *dst = TEXT_OFFSET;
    UINT8 *src = GLYPH_START(ch);
    int i;

    for (i = 0; i < 8; i++)
    {
        *dst = *(src++);
        dst += 80;
    }
}

void print_char_safe(char ch)
{
    UINT16 orig_ipl = set_ipl(7); /* [TO DO] mask less aggressively? */
    print_char(ch);
    set_ipl(orig_ipl);
}

void print_char(char ch)
{
    /* [TO DO] mask VBL for cursor blink?  Already handled elsewhere? */

    /* [TO DO] not currently handled:
        BEL(7),
        TAB(9),
        VT(11),
        DEL(127)
    */

    if (IS_PRINTABLE(ch))
    {
        plot_glyph(ch);
        (*console_x_p)++;

        if (*console_x_p == 80)
        {
            *console_x_p = 0;
            (*console_y_p)++;
        }

        *cursor_visible = 0;
    }
    else if (ch == '\b')
    {
        clear_cursor();

        if (*console_x_p > 0)
            (*console_x_p)--;
        else
        {
            (*console_x_p) = 79;
            (*console_y_p)--;
        }

        plot_glyph(' ');
    }
    else if (ch == CHAR_LF) /* LINE FEED (LF) */
    {
        clear_cursor();
        (*console_y_p)++;
    }
    else if (ch == CHAR_CR) /* CR */
    {
        clear_cursor();
        (*console_x_p) = 0;
    }
    else if (ch == CHAR_FF) /* FORM FEED (FF) */
    {
        clear_screen(get_video_base());
        *console_x_p = 0;
        *console_y_p = 0;
        *cursor_visible = 0;
    }

    if (*console_y_p == 50)
    {
        /* clear_cursor(); <-- [TO DO] needed? */
        (*console_y_p)--;
        scroll();
    }

    reset_cursor();
}

void print_str_safe(char *str)
{
    UINT16 orig_ipl = set_ipl(7); /* [TO DO] mask less aggressively? */
    print_str(str);
    set_ipl(orig_ipl);
}

void print_str(char *str)
{
    register char ch;

    while (ch = *(str++))
        print_char(ch);
}

void do_vbl_isr()
{
    UINT16 orig_ipl = set_ipl(7); /* [TO DO] mask less aggressively? */

    if (*kybd_auto_ch && ++(*kybd_auto_count) > 10)
        input_enqueue(*kybd_auto_ch);

    set_ipl(orig_ipl);

    if (!(++(*vbl_counter) & 0x001F))
    {
        set_ipl(7);
        invert_cursor();
        set_ipl(orig_ipl);
    }
}

void invert_cursor()
{
    UINT8 *base = TEXT_OFFSET;
    int i;

    for (i = 0; i < 8; i++)
    {
        *base ^= 0xFF;
        base += 80;
    }

    *cursor_visible = !(*cursor_visible);
}

void reset_cursor()
{
    if (!(*cursor_visible))
        invert_cursor();

    *vbl_counter = 0;
}

void clear_cursor()
{
    if (*cursor_visible)
        invert_cursor();
}

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

int read_operation_from_floppy(disk_io_request_t *io)
{
    if (setup_dma_for_rw(io->disk, io->side, io->track))
    {
        setup_dma_buffer(io->buffer_address);
        return read_sector(io->sector, io->buffer_address);
    }

    return 0;
}

int write_operation_to_floppy(disk_io_request_t *io)
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
        return read_operation_from_floppy(io);
    }
    else if (io->operation == DISK_OPERATION_WRITE)
    {
        return write_operation_to_floppy(io);
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
