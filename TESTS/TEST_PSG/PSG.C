typedef unsigned char UINT8;
typedef unsigned int UINT16;
typedef unsigned long UINT32;

/* PSG Port A Addresses */
#define PSG_REG_SELECT 0xFF8800
#define PSG_REG_WRITE 0xFF8802

extern UINT16 read_SR(void);
extern void write_SR(UINT16 sr);

UINT16 set_ipl(UINT16 ipl)
{
    UINT16 sr = read_SR();
    UINT16 old_ipl = ((sr >> 8) & 0x0007);

    write_SR((sr & 0xF8FF) | ((ipl & 0x0007) << 8));

    return old_ipl;
}

/* Access PSG registers safely */
UINT8 safe_psg(UINT8 value, UINT8 register_number)
{
    UINT16 orig_ipl = set_ipl(7); /* need to mask both MFP IRQ and VBL IRQ */

    /* Select register and write value */
    *((volatile UINT8 *)PSG_REG_SELECT) = register_number;
    *((volatile UINT8 *)PSG_REG_WRITE) = value;

    (void)set_ipl(orig_ipl);

    return value;
}

/* Function to select a floppy drive */
void select_drive(UINT8 drive, UINT8 side)
{
    /* Read current state of PSG Port A by first selecting register 14 */
    safe_psg(14, 14);

    /* Read the value from the data port */
    UINT8 port_a_value = *((volatile UINT8 *)PSG_REG_WRITE); 

    /* Clear the lower three bits for drive and side selection */
    port_a_value &= 0xF8; /* Preserve the upper bits (3-7) for other functionalities */
    /* Set the lower three bits according to the drive and side selection */
    port_a_value |= (side << 2) | (drive & 0x03);

    /* Use safe_psg to safely write the new value to PSG Port A */
    safe_psg(port_a_value, 14);
}

/* ... rest of your driver code ... */
