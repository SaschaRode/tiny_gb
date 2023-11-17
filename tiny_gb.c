#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define SCREEN_W 160
#define SCREEN_H 144
#define SCREEN_SCALE 3

#define CLOCK_FREQUENCY 4194304

#define MAX_ROM_SIZE (4*1024*1024)
#define MAX_RAM_SIZE (64*1024)
#define MAX_SCANLINE_SPRITES 10

#define LOW(value) ((value) & 0xFF)
#define HIGH(value) ((value >> 8) & 0xFF)
#define COMBINE(high, low) (((high) << 8) | (low))

typedef enum menu_e
{
    MENU_OPEN = 1,
    MENU_RESET,
    MENU_QUIT,
} menu_e;

typedef enum lcd_mode_e
{
    LCD_MODE_HBLANK = 0x00,
    LCD_MODE_VBLANK = 0x01,
    LCD_MODE_SCAN_OAM = 0x02,
    LCD_MODE_PIXEL_TRANSFER = 0x03,
} lcd_mode_e;

typedef struct lcd_control_t
{
    uint8_t bg_and_window_enable : 1;
    uint8_t obj_enable : 1;
    uint8_t obj_size : 1;
    uint8_t bg_tile_map_area : 1;
    uint8_t bg_and_window_tile_data_area : 1;
    uint8_t window_enable : 1;
    uint8_t window_tile_map_area : 1;
    uint8_t enable : 1;
} lcd_control_t;

typedef struct lcd_status_t
{
    uint8_t mode : 2;
    uint8_t lyc_equal_ly : 1;
    uint8_t hblank_interrupt : 1;
    uint8_t vblank_interrupt : 1;
    uint8_t oam_interrupt : 1;
    uint8_t lyc_equal_ly_interrupt : 1;
    uint8_t invalid : 1;
} lcd_status_t;

typedef union palette_t
{
    struct
    {
        uint8_t color0 : 2;
        uint8_t color1 : 2;
        uint8_t color2 : 2;
        uint8_t color3 : 2;
    };
    uint8_t value;
} palette_t;

typedef struct lcd_t
{
    lcd_control_t control;
    lcd_status_t status;
    uint8_t scy;
    uint8_t scx;
    uint8_t ly;
    uint8_t lyc;
    uint8_t dma;
    palette_t bgp;
    palette_t obp0;
    palette_t obp1;
    uint8_t wy;
    uint8_t wx;
} lcd_t;

typedef struct interrupt_t
{
    uint8_t vblank : 1;
    uint8_t stat : 1;
    uint8_t timer : 1;
    uint8_t serial : 1;
    uint8_t joypad : 1;
    uint8_t invalid : 3;
} interrupt_t;

typedef struct timer_control_t
{
    uint8_t clock : 2;
    uint8_t enable : 1;
    uint8_t invalid : 5;
} timer_control_t;

typedef struct timer_t
{
    uint8_t div;
    uint8_t counter;
    uint8_t modulo;
    timer_control_t control;
} timer_t;

typedef struct joypad_t
{
    uint8_t right_or_a : 1;
    uint8_t left_or_b : 1;
    uint8_t up_or_select : 1;
    uint8_t down_or_start : 1;
    uint8_t select_direction : 1;
    uint8_t select_action : 1;
    uint8_t invalid : 2;
} joypad_t;

typedef struct register_flags_t
{
    uint8_t invalid : 4;
    uint8_t c : 1;
    uint8_t h : 1;
    uint8_t n : 1;
    uint8_t z : 1;
} register_flags_t;

typedef struct registers_t
{
    typedef union
    {
        typedef struct
        {
            register_flags_t f;
            uint8_t a;
        };
        uint16_t af;
    };
    typedef union
    {
        typedef struct
        {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };
    typedef union
    {
        typedef struct
        {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };
    typedef union
    {
        typedef struct
        {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };
    uint16_t sp;
    uint16_t pc;
} registers_t;

typedef struct state_t
{
    uint8_t stop : 1;
    uint8_t pending_ime : 1;
    uint8_t ime : 1;
    uint8_t ram : 1;
    uint8_t mbc1_mode : 1;
    uint8_t dma_transfer : 1;
    uint8_t no_vram_access : 1;
    uint8_t no_oam_access : 1;
} state_t;

typedef struct draw_flags_t
{
    uint8_t transparency : 1;
    uint8_t flip : 1;
    uint8_t prio_bg : 1;
    uint8_t invalid : 5;
} draw_flags_t;

typedef struct tile_t
{
    uint16_t lines[8];
} tile_t;

typedef struct sprite_flags_t
{
    uint8_t palette_cgb : 3;
    uint8_t vram_bank_cgb : 1;
    uint8_t palette : 1;
    uint8_t flipx : 1;
    uint8_t flipy : 1;
    uint8_t bg_and_window : 1;
} sprite_flags_t;

typedef struct sprite_attribute_t
{
    uint8_t py;
    uint8_t px;
    uint8_t tile;
    sprite_flags_t flags;
} sprite_attribute_t;

typedef enum cartridge_type_e
{
    CARTRIDGE_TYPE_ROM = 0x00,
    CARTRIDGE_TYPE_MBC1 = 0x01,
    CARTRIDGE_TYPE_MBC1_RAM = 0x02,
    CARTRIDGE_TYPE_MBC1_RAM_BATTERY = 0x03,
    CARTRIDGE_TYPE_MBC2 = 0x05,
    CARTRIDGE_TYPE_MBC2_BATTERY = 0x06,
    CARTRIDGE_TYPE_ROM_RAM = 0x08,
    CARTRIDGE_TYPE_ROM_RAM_BATTERY = 0x09,
    CARTRIDGE_TYPE_MMM01 = 0x0B,
    CARTRIDGE_TYPE_MMM01_RAM = 0x0C,
    CARTRIDGE_TYPE_MMM01_RAM_BATTERY = 0x0D,
    CARTRIDGE_TYPE_MBC3_TIMER_BATTERY = 0x0F,
    CARTRIDGE_TYPE_MBC3_TIMER_RAM_BATTERY = 0x10,
    CARTRIDGE_TYPE_MBC3 = 0x11,
    CARTRIDGE_TYPE_MBC3_RAM = 0x12,
    CARTRIDGE_TYPE_MBC3_RAM_BATTERY = 0x13,
    CARTRIDGE_TYPE_MBC5 = 0x19,
    CARTRIDGE_TYPE_MBC5_RAM = 0x1A,
    CARTRIDGE_TYPE_MBC5_RAM_BATTERY = 0x1B,
    CARTRIDGE_TYPE_MBC5_RUMBLE = 0x1C,
    CARTRIDGE_TYPE_MBC5_RUMBLE_RAM = 0x1D,
    CARTRIDGE_TYPE_MBC5_RUMBLE_RAM_BATTERY = 0x1E,
    CARTRIDGE_TYPE_MBC6 = 0x20,
    CARTRIDGE_TYPE_MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
    CARTRIDGE_TYPE_POCKET_CAMERA = 0xFC,
    CARTRIDGE_TYPE_BANDAI_TAMA5 = 0xFD,
    CARTRIDGE_TYPE_HUC3 = 0xFE,
    CARTRIDGE_TYPE_HUC1_RAM_BATTERY = 0xFF,
} cartridge_type_e;

typedef struct cartridge_header_t
{
    uint8_t entry[4];
    uint16_t logo[24];
    union
    {
        char old_title[16];
        struct
        {
            char title[15];
            uint8_t cgb_mode;
        };
    };
    char licensee[2];
    uint8_t sgb_mode;
    uint8_t type;
    uint8_t rom_size;
    uint8_t ram_size;
    uint8_t destination;
    uint8_t old_licensee;
    uint8_t version;
    uint8_t checksum;
    uint16_t global_checksum;
} cartridge_header_t;

typedef struct cycles_t
{
    uint16_t div;
    uint16_t tac;
    uint16_t dma;
    uint16_t dots;
} cycles_t;

typedef struct gameboy_t
{
    registers_t registers;
    state_t state;
    uint8_t op_cycles;
    cycles_t cycles;
    uint8_t *memory;
    uint8_t *rom;
    uint8_t *ram;
    uint32_t *framebuffer;
    cartridge_header_t *cartridge_header;
    joypad_t *joypad;
    lcd_t *lcd;
    timer_t *timer;
    interrupt_t *interrupt_e;
    interrupt_t *interrupt_f;
    uint8_t *rom_banks[256];
    uint8_t *ram_banks[8];
    uint8_t rom_bank;
    uint8_t ram_bank;
    sprite_attribute_t scanline_sprites[MAX_SCANLINE_SPRITES];
    uint8_t num_scanline_sprites;
} gameboy_t;

static gameboy_t gb;
static uint32_t gb_colors[] = { 0xFFE0F8D0, 0xFF88C070, 0xFF345856, 0xFF081820 };
static char rom_path[MAX_PATH] = { 0 };

static void clear_pixels(uint32_t *framebuffer, uint32_t color)
{
    memset(framebuffer, color, sizeof(uint32_t)*SCREEN_W*SCREEN_H);
}

static void set_pixel(uint32_t *framebuffer, int16_t x, int16_t y, uint32_t color)
{
    if(x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        framebuffer[y*SCREEN_W + x] = color;
}

static uint32_t get_pixel(uint32_t *framebuffer, int16_t x, int16_t y)
{
    uint32_t color = 0;
    if(x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        color = framebuffer[y*SCREEN_W + x];
    return(color);
}

static void load_nintendo_logo(void)
{
    uint16_t *tiles = (uint16_t *)(gb.memory + 0x8000);
    tiles[0] = 0;
    tiles[1] = 0;
    tiles[2] = 0;
    tiles[3] = 0;
    tiles[4] = 0;
    tiles[5] = 0;
    tiles[6] = 0;
    tiles[7] = 0;
    tiles += 8;

    for(uint8_t i = 0; i < 24; i++)
    {
        cartridge_header_t *header = gb.cartridge_header;
        uint8_t tile[] =
        {
            LOW(header->logo[i]),
            HIGH(header->logo[i]),
        };
        for(int8_t j = 0; j < 2; j++)
        {
            for(int8_t k = 1; k >= 0; k--)
            {
                uint16_t line = 0;
                for(int8_t l = 0; l < 4; l++)
                {
                    uint8_t bit = (tile[j] >> (k*4 + l)) & 0x1;
                    line |= (bit*(0x3)) << (2*l);
                }
                tiles[0] = tiles[1] = line;
                tiles += 2;
            }
        }
    }

    tiles[0] = 0x3C;
    tiles[1] = 0x42;
    tiles[2] = 0xB9;
    tiles[3] = 0xA5;
    tiles[4] = 0xB9;
    tiles[5] = 0xA5;
    tiles[6] = 0x42;
    tiles[7] = 0x3C;

    uint8_t *tilemap = gb.memory + 0x9800;
    uint8_t id = 1;
    for(uint8_t y = 8; y < 10; y++)
    {
        for(uint8_t x = 4; x < 16; x++)
        {
            tilemap[32*y + x] = id++;
        }
    }
    tilemap[32*8 + 16] = 25;
}

static uint16_t rom_kib(void)
{
    uint16_t size = 32*(1 << gb.cartridge_header->rom_size);
    return(size);
}

static uint16_t ram_kib(void)
{
    uint16_t size = 0;
    switch(gb.cartridge_header->ram_size)
    {
        case 0x2: size = 8; break;
        case 0x3: size = 32; break;
        case 0x4: size = 128; break;
        case 0x5: size = 64; break;
    }
    return(size);
}

static void load_ram(char *path)
{
    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(file != INVALID_HANDLE_VALUE)
    {
        uint32_t size = GetFileSize(file, NULL);
        assert(size <= MAX_RAM_SIZE);
        ReadFile(file, gb.ram, size, NULL, 0);
        memcpy(gb.memory + 0xA000, gb.ram_banks[gb.ram_bank], 0x2000);
        CloseHandle(file);
    }
}

static void save_ram(char *path)
{
    HANDLE file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
    if(file != INVALID_HANDLE_VALUE)
    {
        uint16_t size = 1024*ram_kib();
        if(size > 0)
        {
            memcpy(gb.ram_banks[gb.ram_bank], gb.memory + 0xA000, 0x2000);
            WriteFile(file, gb.ram, size, NULL, 0);
            CloseHandle(file);
        }
    }
}

static void reset(void)
{
    memset(&gb.registers, 0, sizeof(registers_t));
    gb.registers.af = 0x01B0,
    gb.registers.bc = 0x0013,
    gb.registers.de = 0x00D8,
    gb.registers.hl = 0x014D,
    gb.registers.sp = 0xFFFE,
    gb.registers.pc = 0x0100,
    gb.op_cycles = 0;

    memset(&gb.state, 0, sizeof(state_t));
    gb.state.ime = 1;

    memset(&gb.cycles, 0, sizeof(cycles_t));

    memset(gb.memory, 0, 0x10000);
    gb.memory[0xFF00] = 0xCF;
    gb.memory[0xFF02] = 0x7E;
    gb.memory[0xFF04] = 0xAB;
    gb.memory[0xFF07] = 0xF8;
    gb.memory[0xFF0F] = 0xE1;
    gb.memory[0xFF40] = 0x91;
    gb.memory[0xFF41] = 0x80;
    gb.memory[0xFF46] = 0xFF;
    gb.memory[0xFF47] = 0xFC;
    gb.memory[0xFF48] = 0xFF;
    gb.memory[0xFF49] = 0xFF;
    gb.memory[0xFF4D] = 0xFF;
    gb.memory[0xFF4F] = 0xFF;
    gb.memory[0xFF70] = 0xFF;

    gb.rom_bank = 0;
    gb.ram_bank = 0;
    gb.num_scanline_sprites = 0;

    clear_pixels(gb.framebuffer, gb_colors[0]);

    if(strlen(rom_path) > 0)
    {
        memcpy(gb.memory, gb.rom, 0x8000);
        char path[MAX_PATH] = { 0 };
        strcat(path, rom_path);
        strcat(path, ".sav");
        load_ram(path);
        load_nintendo_logo();
    }
}

static void load(char *path)
{
    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(file != INVALID_HANDLE_VALUE)
    {
        strcpy(rom_path, path);
        uint32_t size = GetFileSize(file, NULL);
        assert(size <= MAX_ROM_SIZE);
        ReadFile(file, gb.rom, size, NULL, 0);
        uint8_t checksum = 0;
        for(uint16_t address = 0x0134; address <= 0x014C; address++)
            checksum = checksum - gb.rom[address] - 1;
        if(checksum != gb.rom[0x14D])
            rom_path[0] = '\0';
        CloseHandle(file);
    }
    reset();
}

static void save(void)
{
    if(strlen(rom_path) > 0)
    {
        if(gb.cartridge_header->type == CARTRIDGE_TYPE_MBC1_RAM_BATTERY)
        {
            char path[MAX_PATH] = { 0 };
            strcat(path, rom_path);
            strcat(path, ".sav");
            save_ram(path);
        }
    }
}

static uint8_t palette_color(palette_t palette, uint8_t idx)
{
    uint8_t color = ((palette.value >> (2*idx)) & 0x3);
    return(color);
}

static void draw_tile_on_scanline(int16_t x, int16_t y, uint16_t line, palette_t palette, draw_flags_t flags)
{
    uint8_t low = LOW(line);
    uint8_t high = HIGH(line);
    for(uint8_t i = 0; i <= 7; i++)
    {
        uint8_t idx = (((low >> i) & 0x01) | (((high >> i) & 0x01)) << 1);
        uint8_t color = palette_color(palette, idx);
        int16_t px = (flags.flip ? (x + i) : (x + (7 - i)));
        if((!flags.transparency || idx != 0) && (!flags.prio_bg || get_pixel(gb.framebuffer, px, y) == gb_colors[gb.lcd->bgp.color0]))
            set_pixel(gb.framebuffer, px, y, gb_colors[color]);
    }
}

static void set_mode(lcd_mode_e mode)
{
    if(mode == LCD_MODE_HBLANK)
    {
        gb.state.no_oam_access = 0;
        gb.state.no_vram_access = 0;
        if(gb.lcd->status.hblank_interrupt)
            gb.interrupt_f->stat = 1;
    }
    else if(mode == LCD_MODE_PIXEL_TRANSFER)
    {
        gb.state.no_oam_access = 1;
        gb.state.no_vram_access = 1;
    }
    else if(mode == LCD_MODE_SCAN_OAM)
    {
        gb.state.no_oam_access = 1;
        gb.state.no_vram_access = 0;
        if(gb.lcd->status.oam_interrupt)
            gb.interrupt_f->stat = 1;
    }
    else if(mode == LCD_MODE_VBLANK)
    {
        gb.state.no_oam_access = 0;
        gb.state.no_vram_access = 0;
        gb.interrupt_f->vblank = 1;
    }
    gb.lcd->status.mode = mode;
}

static void set_ly(uint8_t value)
{
    if(gb.lcd->ly != value)
    {
        gb.lcd->ly = value;
        gb.lcd->status.lyc_equal_ly = (gb.lcd->ly == gb.lcd->lyc);
        if(gb.lcd->status.lyc_equal_ly && gb.lcd->status.lyc_equal_ly_interrupt)
            gb.interrupt_f->stat = 1;
    }
}

static void set_rom_bank(uint8_t bank)
{
    if(bank != gb.rom_bank)
    {
        uint16_t size = rom_kib();
        uint8_t num = (uint8_t)(size/16);
        gb.rom_bank = min(max(bank, 1), (num - 1));
        memcpy(gb.memory + 0x4000, gb.rom_banks[gb.rom_bank], 0x4000);
    }
}

static void set_ram_bank(uint8_t bank)
{
    if(bank != gb.ram_bank)
    {
        memcpy(gb.ram_banks[gb.ram_bank], gb.memory + 0xA000, 0x2000);
        memcpy(gb.memory + 0xA000, gb.ram_banks[bank], 0x2000);
        gb.ram_bank = bank;
    }
}

static void mem_w(uint16_t address, uint8_t value)
{
    if(!gb.state.dma_transfer || (address >= 0xFF80 && address <= 0xFFFE))
    {
        if(address >= 0x0000 && address <= 0x1FFF)
        {
            gb.state.ram = (value & 0x0F) == 0x0A;
        }
        else if(address >= 0x2000 && address <= 0x3FFF)
        {
            uint8_t bank = ((gb.rom_bank & 0xE0) | (value & 0x1F));
            set_rom_bank(bank);
        }
        else if(address >= 0x4000 && address <= 0x5FFF)
        {
            uint8_t bank = (value & 0x3);
            if(gb.state.mbc1_mode)
            {
                set_ram_bank(bank);
            }
            else
            {
                bank = ((gb.rom_bank & 0x1F) | (bank << 5));
                set_rom_bank(bank);
            }
        }
        else if(address >= 0x6000 && address <= 0x7FFF)
        {
            gb.state.mbc1_mode = ((value & 0x1) != 0);
        }
        else if(address >= 0x8000 && address <= 0x9FFF)
        {
            if(!gb.state.no_vram_access)
            {
                gb.memory[address] = value;
            }
        }
        else if(address >= 0xA000 && address <= 0xBFFF)
        {
            if(gb.state.ram)
                gb.memory[address] = value;
        }
        else if(address >= 0xC000 && address <= 0xDFFF)
        {
            gb.memory[address] = value;
            if(address <= 0xDDFF)
                gb.memory[address + 0x2000] = value;
        }
        else if(address >= 0xFE00 && address <= 0xFE9F)
        {
            if(!gb.state.no_oam_access)
            {
                gb.memory[address] = value;
            }
        }
        else if(address >= 0xFF00 && address <= 0xFF7F)
        {
            uint8_t old_value = gb.memory[address];
            gb.memory[address] = value;
            switch(address)
            {
                case 0xFF00:
                {
                    gb.memory[address] = (0xC0 | (value & 0x30) | (old_value & 0x0F));
                    if(gb.joypad->select_direction == 0)
                    {
                        gb.joypad->right_or_a = !((GetKeyState(VK_RIGHT) & 0x8000) != 0);
                        gb.joypad->left_or_b = !((GetKeyState(VK_LEFT) & 0x8000) != 0);
                        gb.joypad->up_or_select = !((GetKeyState(VK_UP) & 0x8000) != 0);
                        gb.joypad->down_or_start = !((GetKeyState(VK_DOWN) & 0x8000) != 0);
                    }
                    else if(gb.joypad->select_action == 0)
                    {
                        gb.joypad->right_or_a = !((GetKeyState('S') & 0x8000) != 0);
                        gb.joypad->left_or_b = !((GetKeyState('A') & 0x8000) != 0);
                        gb.joypad->up_or_select = !((GetKeyState(VK_SHIFT) & 0x8000) != 0);
                        gb.joypad->down_or_start = !((GetKeyState(VK_RETURN) & 0x8000) != 0);
                    }
                    for(uint8_t i = 0; i < 4; i++)
                    {
                        if(((old_value & (1 << i)) != 0) && ((gb.memory[address] & (1 << i)) == 0))
                            gb.interrupt_f->joypad = 1;
                    }
                    break;
                }
                case 0xFF04:
                {
                    gb.memory[address] = 0;
                    gb.cycles.div = 0;
                    break;
                }
                case 0xFF40:
                {
                    if(!((lcd_control_t *)&value)->enable && ((lcd_control_t *)&old_value)->enable)
                    {
                        clear_pixels(gb.framebuffer, gb_colors[0]);
                        set_mode(LCD_MODE_HBLANK);
                        set_ly(0);
                        gb.cycles.dots = 0;
                    }
                    break;
                }
                case 0xFF41:
                {
                    gb.memory[address] = (0x80 | value);
                    break;
                }
                case 0xFF46:
                {
                    gb.state.dma_transfer = 1;
                    gb.cycles.dma = 0;
                    break;
                }
                case 0xFF0F:
                {
                    gb.memory[address] = (0xE0 | value);
                    break;
                }
            }
        }
        else if(address >= 0xFF80 && address <= 0xFFFF)
        {
            gb.memory[address] = value;
        }
    }
}

static uint8_t mem_r(uint16_t address)
{
    uint8_t value = 0xFF;
    if(!((gb.state.no_oam_access) && (address >= 0xFE00 && address <= 0xFE9F)) &&
        !(gb.state.no_vram_access && (address >= 0x8000 && address <= 0x9FFF)) &&
        !(gb.state.dma_transfer && (address < 0xFF80 || address > 0xFFFE)))
    {
        value = gb.memory[address];
    }
    return(value);
}

static uint8_t r8_low_r(uint8_t op)
{
    uint8_t r = 0xFF;
    switch((op & 0x0F))
    {
        case 0x00: case 0x08: r = gb.registers.b; break;
        case 0x01: case 0x09: r = gb.registers.c; break;
        case 0x02: case 0x0A: r = gb.registers.d; break;
        case 0x03: case 0x0B: r = gb.registers.e; break;
        case 0x04: case 0x0C: r = gb.registers.h; break;
        case 0x05: case 0x0D: r = gb.registers.l; break;
        case 0x06: case 0x0E: r = mem_r(gb.registers.hl); gb.op_cycles += 4; break;
        case 0x07: case 0x0F: r = gb.registers.a; break;
    }
    return(r);
}

static void r8_low_w(uint8_t op, uint8_t value)
{
    switch((op & 0x0F))
    {
        case 0x00: case 0x08: gb.registers.b = value; break;
        case 0x01: case 0x09: gb.registers.c = value; break;
        case 0x02: case 0x0A: gb.registers.d = value; break;
        case 0x03: case 0x0B: gb.registers.e = value; break;
        case 0x04: case 0x0C: gb.registers.h = value; break;
        case 0x05: case 0x0D: gb.registers.l = value; break;
        case 0x06: case 0x0E: mem_w(gb.registers.hl, value); gb.op_cycles += 4; break;
        case 0x07: case 0x0F: gb.registers.a = value; break;
    }
}

static uint8_t r8_high_r(uint8_t op)
{
    uint8_t r = 0xFF;
    switch((op & 0xF0))
    {
        case 0x00: case 0x40: r = ((op & 0x0F) <= 0x07 ? gb.registers.b : gb.registers.c); break;
        case 0x10: case 0x50: r = ((op & 0x0F) <= 0x07 ? gb.registers.d : gb.registers.e); break;
        case 0x20: case 0x60: r = ((op & 0x0F) <= 0x07 ? gb.registers.h : gb.registers.l); break;
        case 0x30: case 0x70:
        {
            if((op & 0x0F) <= 0x07)
            {
                r = mem_r(gb.registers.hl);
                gb.op_cycles += 4;
            }
            else
            {
                r = gb.registers.a;
            }
            break;
        }
    }
    return(r);
}

static void r8_high_w(uint8_t op, uint8_t value)
{
    switch((op & 0xF0))
    {
        case 0x00: case 0x40:
        {
            if((op & 0x0F) <= 0x07)
                gb.registers.b = value;
            else
                gb.registers.c = value;
             break;
        }
        case 0x10: case 0x50:
        {
            if((op & 0x0F) <= 0x07)
                gb.registers.d = value;
            else
                gb.registers.e = value;
             break;
        }
        case 0x20: case 0x60:
        {
            if((op & 0x0F) <= 0x07)
                gb.registers.h = value;
            else
                gb.registers.l = value;
             break;
        }
        case 0x30: case 0x70:
        {
            if((op & 0x0F) <= 0x07)
            {
                mem_w(gb.registers.hl, value);
                gb.op_cycles += 4;
            }
            else
            {
                gb.registers.a = value;
            }
            break;
        }
    }
}

static uint16_t *r16_rw(uint8_t op)
{
    uint16_t *r = NULL;
    switch((op & 0xF0))
    {
        case 0x00: case 0xC0: r = &gb.registers.bc; break;
        case 0x10: case 0xD0: r = &gb.registers.de; break;
        case 0x20: case 0xE0: r = &gb.registers.hl; break;
        case 0x30: r = &gb.registers.sp; break;
        case 0xF0: r = &gb.registers.af; break;
    }
    return(r);
}

static bool condition(uint8_t op)
{
    bool result = false;
    switch(op)
    {
        case 0x20: case 0xC0: case 0xC2: case 0xC4: result = (gb.registers.f.z == 0); break;
        case 0x30: case 0xD0: case 0xD2: case 0xD4: result = (gb.registers.f.c == 0); break;
        case 0x28: case 0xC8: case 0xCA: case 0xCC: result = (gb.registers.f.z == 1); break;
        case 0x38: case 0xD8: case 0xDA: case 0xDC: result = (gb.registers.f.c == 1); break;
    }
    return(result);
}

static void execute_cb_op(uint8_t op)
{
    switch((op & 0xF0))
    {
        // RLC/RRC
        case 0x00:
        {
            if((op & 0x0F) <= 0x07)
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit7 = (old_value & 0x80) != 0;
                uint8_t value = ((old_value << 1) | bit7);
                r8_low_w(op, value);
                gb.registers.f.c = bit7;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            else
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit0 = (old_value & 0x01) != 0;
                uint8_t value = ((old_value >> 1) | (bit0 << 7));
                r8_low_w(op, value);
                gb.registers.f.c = bit0;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            break;
        }
        // RL/RR
        case 0x10:
        {
            if((op & 0x0F) <= 0x07)
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit7 = (old_value & 0x80) != 0;
                uint8_t value = ((old_value << 1) | gb.registers.f.c);
                r8_low_w(op, value);
                gb.registers.f.c = bit7;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            else
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit0 = (old_value & 0x01) != 0;
                uint8_t value = ((old_value >> 1) | (gb.registers.f.c << 7));
                r8_low_w(op, value);
                gb.registers.f.c = bit0;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            break;
        }
        // SLA/SRA
        case 0x20:
        {
            if((op & 0x0F) <= 0x07)
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit7 = (old_value & 0x80) != 0;
                uint8_t value = (old_value << 1);
                r8_low_w(op, value);
                gb.registers.f.c = bit7;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            else
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit0 = (old_value & 0x01) != 0;
                uint8_t bit7 = (old_value & 0x80) != 0;
                uint8_t value = ((old_value >> 1) | (bit7 << 7));
                r8_low_w(op, value);
                gb.registers.f.c = bit0;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            break;
        }
        // SWAP/SRL
        case 0x30:
        {
            if((op & 0x0F) <= 0x07)
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t low = (old_value & 0x0F);
                uint8_t high = (old_value >> 4);
                uint8_t value = (low << 4 | high);
                r8_low_w(op, value);
                gb.registers.f.c = 0;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            else
            {
                uint8_t old_value = r8_low_r(op);
                uint8_t bit0 = (old_value & 0x01) != 0;
                uint8_t value = (old_value >> 1);
                r8_low_w(op, value);
                gb.registers.f.c = bit0;
                gb.registers.f.h = 0;
                gb.registers.f.n = 0;
                gb.registers.f.z = (value == 0);
                gb.op_cycles += 4;
            }
            break;
        }
        // BIT
        case 0x40: case 0x50: case 0x60: case 0x70:
        {
            uint8_t value = r8_low_r(op);
            uint8_t bit = (((op >> 3) & 0x7) | ((op & 0x8) >> 3));
            gb.registers.f.h = 1;
            gb.registers.f.n = 0;
            gb.registers.f.z = ((value & (1 << bit)) == 0);
            gb.op_cycles += 4;
            break;
        }
        // RES
        case 0x80: case 0x90: case 0xA0: case 0xB0:
        {
            uint8_t value = r8_low_r(op);
            uint8_t bit = (((op >> 3) & 0x7) | ((op & 0x8) >> 3));
            value &= ~(1 << bit);
            r8_low_w(op, value);
            gb.op_cycles += 4;
            break;
        }
        // SET
        case 0xC0: case 0xD0: case 0xE0: case 0xF0:
        {
            uint8_t value = r8_low_r(op);
            uint8_t bit = (((op >> 3) & 0x7) | ((op & 0x8) >> 3));
            value |= (1 << bit);
            r8_low_w(op, value);
            gb.op_cycles += 4;
            break;
        }
        default:
        {
            DebugBreak();
            break;
        }
    }
}

static void execute_op(uint8_t op)
{
    switch(op)
    {
        // NOP
        case 0x00:
        {
            gb.op_cycles += 4;
            break;
        }
        // STOP
        case 0x10:
        {
            gb.state.stop = !gb.state.stop;
            mem_w(0xFF04, 0);
            gb.op_cycles += 4;
            break;
        }
        // HALT
        case 0x76:
        {
            // TODO: Handle HALT instruction properly.
            gb.op_cycles += 4;
            break;
        }
        // RLCA, ELA, RRCA, RRA
        case 0x07: case 0x17: case 0x0F: case 0x1F:
        {
            execute_cb_op(op);
            gb.registers.f.z = 0;
            break;
        }
        // JR i8
        case 0x18:
        {
            int8_t value = (int8_t)mem_r(gb.registers.pc++);
            gb.registers.pc += value;
            gb.op_cycles += 12;
            break;
        }
        // JR condition, i8
        case 0x20: case 0x28: case 0x30: case 0x38:
        {
            int8_t value = (int8_t)mem_r(gb.registers.pc++);
            if(condition(op))
            {
                gb.registers.pc += value;
                gb.op_cycles += 4;
            }
            gb.op_cycles += 8;
            break;
        }
        // DAA
        case 0x27:
        {
            if(gb.registers.f.n)
            {
                if(gb.registers.f.c)
                    gb.registers.a -= 0x60;
                if(gb.registers.f.h)
                    gb.registers.a -= 0x06;
            }
            else
            {
                if(gb.registers.f.c || (gb.registers.a > 0x99))
                {
                    gb.registers.a += 0x60;
                    gb.registers.f.c = 1;
                }
                if(gb.registers.f.h || ((gb.registers.a & 0x0F) > 0x09))
                {
                    gb.registers.a += 0x06;
                }
            }
            gb.registers.f.z = (gb.registers.a == 0);
            gb.registers.f.h = 0;
            gb.op_cycles += 4;
            break;
        }
        // CPL
        case 0x2F:
        {
            gb.registers.a = ~gb.registers.a;
            gb.registers.f.h = 1;
            gb.registers.f.n = 1;
            gb.op_cycles += 4;
            break;
        }
        // SCF
        case 0x37:
        {
            gb.registers.f.c = 1;
            gb.registers.f.h = 0;
            gb.registers.f.n = 0;
            gb.op_cycles += 4;
            break;
        }
        // CCF
        case 0x3F:
        {
            gb.registers.f.c = !gb.registers.f.c;
            gb.registers.f.h = 0;
            gb.registers.f.n = 0;
            gb.op_cycles += 4;
            break;
        }
        // INC r16
        case 0x03: case 0x13: case 0x23: case 0x33:
        {
            *r16_rw(op) += 1;
            gb.op_cycles += 8;
            break;
        }
        // INC r8
        case 0x04: case 0x14: case 0x24: case 0x34: case 0x0C: case 0x1C: case 0x2C: case 0x3C:
        {
            uint8_t old_value  = r8_high_r(op);
            uint8_t value = old_value + 1;
            r8_high_w(op, value);
            gb.registers.f.h = ((value & 0xF0) != (old_value & 0xF0));
            gb.registers.f.n = 0;
            gb.registers.f.z = (value == 0);
            gb.op_cycles += 4;
            break;
        }
        // DEC r16
        case 0x0B: case 0x1B: case 0x2B: case 0x3B:
        {
            *r16_rw(op) -= 1;
            gb.op_cycles += 8;
            break;
        }
        // DEC r8
        case 0x05: case 0x15: case 0x25:  case 0x35: case 0x0D: case 0x1D: case 0x2D:  case 0x3D:
        {
            uint8_t value = r8_high_r(op);
            value -= 1;
            r8_high_w(op, value);
            gb.registers.f.h = ((value & 0x0F) == 0x0F);
            gb.registers.f.n = 1;
            gb.registers.f.z = (value == 0);
            gb.op_cycles += 4;
            break;
        }
        // LD r8, u8
        case 0x06: case 0x16: case 0x26: case 0x36: case 0x0E: case 0x1E: case 0x2E: case 0x3E:
        {
            uint8_t value = mem_r(gb.registers.pc++);
            r8_high_w(op, value);
            gb.op_cycles += 8;
            break;
        }
        // LD (u16), SP
        case 0x08:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            uint16_t address = COMBINE(high, low);
            mem_w(address, LOW(gb.registers.sp));
            mem_w(address + 1, HIGH(gb.registers.sp));
            gb.op_cycles += 20;
            break;
        }
        // LD A, (r16)
        case 0x0A: case 0x1A:
        {
            uint16_t value = *r16_rw(op);
            gb.registers.a = mem_r(value);
            gb.op_cycles += 8;
            break;
        }
        // LD A, (HL+)
        case 0x2A:
        {
            gb.registers.a = mem_r(gb.registers.hl++);
            gb.op_cycles += 8;
            break;
        }
        // LD A, (HL-)
        case 0x3A:
        {
            gb.registers.a = mem_r(gb.registers.hl--);
            gb.op_cycles += 8;
            break;
        }
        // LD r16, u16
        case 0x01: case 0x11: case 0x21: case 0x31:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            *r16_rw(op) = COMBINE(high, low);
            gb.op_cycles += 12;
            break;
        }
        // LD (r8), A
        case 0x02: case 0x12:
        {
            uint16_t address = *r16_rw(op);
            mem_w(address, gb.registers.a);
            gb.op_cycles += 8;
            break;
        }
        // LD (HL+), A
        case 0x22:
        {
            mem_w(gb.registers.hl++, gb.registers.a);
            gb.op_cycles += 8;
            break;
        }
        // LD (HL-), A
        case 0x32:
        {
            mem_w(gb.registers.hl--, gb.registers.a);
            gb.op_cycles += 8;
            break;
        }
        // LD r8, r8
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
        case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        {
            uint8_t value = r8_low_r(op);
            r8_high_w(op, value);
            gb.op_cycles += 4;
            break;
        }
        // LD (FF00+u8), A
        case 0xE0:
        {
            uint8_t value = mem_r(gb.registers.pc++);
            mem_w((0xFF00 + value), gb.registers.a);
            gb.op_cycles += 12;
            break;
        }
        // LD A, (FF00+u8)
        case 0xF0:
        {
            uint8_t value = mem_r(gb.registers.pc++);
            gb.registers.a = mem_r((0xFF00 + value));
            gb.op_cycles += 12;
            break;
        }
        // LD (FF00+C), A
        case 0xE2:
        {
            mem_w((0xFF00 + gb.registers.c), gb.registers.a);
            gb.op_cycles += 8;
            break;
        }
        // LD A, (FF00+C)
        case 0xF2:
        {
            gb.registers.a = mem_r((0xFF00 + gb.registers.c));
            gb.op_cycles += 8;
            break;
        }
        // LD (u16), A
        case 0xEA:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            uint16_t address = COMBINE(high, low);
            mem_w(address, gb.registers.a);
            gb.op_cycles += 16;
            break;
        }
        // LD A, (u16)
        case 0xFA:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            uint16_t address = COMBINE(high, low);
            gb.registers.a = mem_r(address);
            gb.op_cycles += 16;
            break;
        }
        // LD SP, HL
        case 0xF9:
        {
            gb.registers.sp = gb.registers.hl;
            gb.op_cycles += 8;
            break;
        }
        // ADD SP, i8
        case 0xE8:
        {
            uint16_t old_value = gb.registers.sp;
            int8_t value = (int8_t)mem_r(gb.registers.pc++);
            gb.registers.sp += value;
            gb.registers.f.c = (gb.registers.sp < old_value);
            gb.registers.f.h = ((gb.registers.sp & 0x0F) < (old_value & 0x0F));
            gb.registers.f.n = 0;
            gb.registers.f.z = 0;
            gb.op_cycles += 16;
            break;
        }
        // LD HL, SP+i8
        case 0xF8:
        {
            int8_t value = (int8_t)mem_r(gb.registers.pc++);
            gb.registers.hl = gb.registers.sp + value;
            gb.registers.f.c = (gb.registers.hl < gb.registers.sp);
            gb.registers.f.h = ((gb.registers.hl & 0x0F) < (gb.registers.sp & 0x0F));
            gb.registers.f.n = 0;
            gb.registers.f.z = 0;
            gb.op_cycles += 12;
            break;
        }
        // ADD HL, r16
        case 0x09: case 0x19: case 0x29: case 0x39:
        {
            uint16_t old_value = gb.registers.hl;
            gb.registers.hl += *r16_rw(op);
            gb.registers.f.c = (gb.registers.hl < old_value);
            gb.registers.f.h = ((gb.registers.hl & 0x00FF) < (old_value & 0x00FF));
            gb.registers.f.n = 0;
            gb.op_cycles += 8;
            break;
        }
        // ADD A, r8/u8
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: case 0xC6:
        {
            uint8_t old_value = gb.registers.a;
            gb.registers.a += ((op == 0xC6) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            gb.registers.f.c = (gb.registers.a < old_value);
            gb.registers.f.h = ((gb.registers.a & 0x0F) < (old_value & 0x0F));
            gb.registers.f.n = 0;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xC6) ? 8 : 4);
            break;
        }
        // ADC A, r8/u8
        case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: case 0xCE:
        {
            uint8_t value = ((op == 0xCE) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            uint16_t a = gb.registers.a + value + gb.registers.f.c;
            uint16_t a_nibble = (gb.registers.a & 0x0F) + (value & 0x0F) + gb.registers.f.c;
            gb.registers.a = (uint8_t)a;
            gb.registers.f.c = a > 0xFF;
            gb.registers.f.h = a_nibble > 0x0F;
            gb.registers.f.n = 0;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xCE) ? 8 : 4);
            break;
        }
        // SUB A, r8/u8
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: case 0xD6:
        {
            uint8_t old_value = gb.registers.a;
            gb.registers.a -= ((op == 0xD6) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            gb.registers.f.c = (gb.registers.a > old_value);
            gb.registers.f.h = ((gb.registers.a & 0x0F) > (old_value & 0x0F));
            gb.registers.f.n = 1;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xD6) ? 8 : 4);
            break;
        }
        // SBC A, r8/u8
        case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F: case 0xDE:
        {
            uint8_t value = ((op == 0xDE) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            int16_t a = gb.registers.a - value - gb.registers.f.c;
            int16_t a_nibble = (gb.registers.a & 0x0F) - (value & 0x0F) - gb.registers.f.c;
            gb.registers.a = (uint8_t)a;
            gb.registers.f.c = a < 0;
            gb.registers.f.h = a_nibble < 0;
            gb.registers.f.n = 1;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xDE) ? 8 : 4);
            break;
        }
        // AND A, r8/u8
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xE6:
        {
            gb.registers.a &= ((op == 0xE6) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            gb.registers.f.c = 0;
            gb.registers.f.h = 1;
            gb.registers.f.n = 0;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xE6) ? 8 : 4);
            break;
        }
        // XOR A, r8/u8
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF: case 0xEE:
        {
            gb.registers.a ^= ((op == 0xEE) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            gb.registers.f.c = 0;
            gb.registers.f.h = 0;
            gb.registers.f.n = 0;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xEE) ? 8 : 4);
            break;
        }
        // OR A, r8/u8
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: case 0xF6:
        {
            gb.registers.a |= ((op == 0xF6) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            gb.registers.f.c = 0;
            gb.registers.f.h = 0;
            gb.registers.f.n = 0;
            gb.registers.f.z = (gb.registers.a == 0);
            gb.op_cycles += ((op == 0xF6) ? 8 : 4);
            break;
        }
        // CP A, r8/u8
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: case 0xFE:
        {
            uint8_t value = ((op == 0xFE) ? mem_r(gb.registers.pc++) : r8_low_r(op));
            gb.registers.f.c = (gb.registers.a < value);
            gb.registers.f.h = ((gb.registers.a & 0x0F) < (value & 0x0F));
            gb.registers.f.n = 1;
            gb.registers.f.z = (gb.registers.a == value);
            gb.op_cycles += ((op == 0xFE) ? 8 : 4);
            break;
        }
        // POP
        case 0xC1: case 0xD1: case 0xE1: case 0xF1:
        {
            uint8_t low = mem_r(gb.registers.sp++);
            uint8_t high = mem_r(gb.registers.sp++);
            *r16_rw(op) = COMBINE(high, low);
            gb.op_cycles += 12;
            break;
        }
        // PUSH
        case 0xC5: case 0xD5: case 0xE5: case 0xF5:
        {
            uint16_t value = *r16_rw(op);
            mem_w(--gb.registers.sp, HIGH(value));
            mem_w(--gb.registers.sp, LOW(value));
            gb.op_cycles += 16;
            break;
        }
        // JP u16
        case 0xC3:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            gb.registers.pc = COMBINE(high, low);
            gb.op_cycles += 16;
            break;
        }
        // JP condition, u16
        case 0xC2: case 0xCA: case 0xD2: case 0xDA:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            if(condition(op))
            {
                gb.registers.pc = COMBINE(high, low);
                gb.op_cycles += 4;
            }
            gb.op_cycles += 12;
            break;
        }
        // JP HL
        case 0xE9:
        {
            gb.registers.pc = gb.registers.hl;
            gb.op_cycles += 4;
            break;
        }
        // RET
        case 0xC9:
        {
            uint8_t low = mem_r(gb.registers.sp++);
            uint8_t high = mem_r(gb.registers.sp++);
            gb.registers.pc = COMBINE(high, low);
            gb.op_cycles += 16;
            break;
        }
        // RET condition
        case 0xC0: case 0xC8: case 0xD0: case 0xD8:
        {
            if(condition(op))
            {
                uint8_t low = mem_r(gb.registers.sp++);
                uint8_t high = mem_r(gb.registers.sp++);
                gb.registers.pc = COMBINE(high, low);
                gb.op_cycles += 12;
            }
            gb.op_cycles += 8;
            break;
        }
        // RETI
        case 0xD9:
        {
            gb.state.ime = 1;
            uint8_t low = mem_r(gb.registers.sp++);
            uint8_t high = mem_r(gb.registers.sp++);
            gb.registers.pc = COMBINE(high, low);
            gb.op_cycles += 16;
            break;
        }
        // PREFIX CB
        case 0xCB:
        {
            execute_cb_op(mem_r(gb.registers.pc++));
            gb.op_cycles += 4;
            break;
        }
        // CALL u16
        case 0xCD:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            mem_w(--gb.registers.sp, HIGH(gb.registers.pc));
            mem_w(--gb.registers.sp, LOW(gb.registers.pc));
            gb.registers.pc = COMBINE(high, low);
            gb.op_cycles += 24;
            break;
        }
        // CALL condition, u16
        case 0xC4: case 0xCC: case 0xD4: case 0xDC:
        {
            uint8_t low = mem_r(gb.registers.pc++);
            uint8_t high = mem_r(gb.registers.pc++);
            if(condition(op))
            {
                mem_w(--gb.registers.sp, HIGH(gb.registers.pc));
                mem_w(--gb.registers.sp, LOW(gb.registers.pc));
                gb.registers.pc = COMBINE(high, low);
                gb.op_cycles += 12;
            }
            gb.op_cycles += 12;
            break;
        }
        // RST 00h-38h
        case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF:
        {
            mem_w(--gb.registers.sp, HIGH(gb.registers.pc));
            mem_w(--gb.registers.sp, LOW(gb.registers.pc));
            gb.registers.pc = (op & 0x38);
            gb.op_cycles += 16;
            break;
        }
        // DI
        case 0xF3:
        {
            gb.state.ime = 0;
            gb.op_cycles += 4;
            break;
        }
        // EI
        case 0xFB:
        {
            gb.state.pending_ime = 1;
            gb.op_cycles += 4;
            break;
        }
        // INVALID
        case 0xD3: case 0xDB: case 0xDD: case 0xE3: case 0xE4: case 0xEB: case 0xEC: case 0xED: case 0xF4: case 0xFC: case 0xFD:
        {
            break;
        }
        default:
        {
            DebugBreak();
            break;
        }
    }
}

static void check_interrupt(void)
{
    if(gb.state.ime)
    {
        uint16_t interrupt = 0;
        if(gb.interrupt_e->vblank && gb.interrupt_f->vblank)
        {
            interrupt = 0x40;
            gb.interrupt_f->vblank = 0;
        }
        else if(gb.interrupt_e->stat && gb.interrupt_f->stat)
        {
            interrupt = 0x48;
            gb.interrupt_f->stat = 0;
        }
        else if(gb.interrupt_e->timer && gb.interrupt_f->timer)
        {
            interrupt = 0x50;
            gb.interrupt_f->timer = 0;
        }
        else if(gb.interrupt_e->joypad && gb.interrupt_f->joypad)
        {
            interrupt = 0x60;
            gb.interrupt_f->joypad = 0;
        }

        if(interrupt)
        {
            gb.memory[--gb.registers.sp] = HIGH(gb.registers.pc);
            gb.memory[--gb.registers.sp] = LOW(gb.registers.pc);
            gb.registers.pc = interrupt;
            gb.op_cycles += 20;
            gb.state.ime = 0;
        }
    }

    if(gb.state.pending_ime)
    {
        gb.state.ime = 1;
        gb.state.pending_ime = 0;
    }
}

static void scan_oam(void)
{
    sprite_attribute_t *sprites = (sprite_attribute_t *)(gb.memory + 0xFE00);
    gb.num_scanline_sprites = 0;
    for(uint8_t i = 0; i < 40; i++)
    {
        sprite_attribute_t *sprite = &sprites[i];
        uint8_t y = (sprite->py - 16);
        uint8_t size = (gb.lcd->control.obj_size ? 16 : 8);
        if(gb.lcd->ly >= y && gb.lcd->ly < (y + size))
        {
            gb.scanline_sprites[gb.num_scanline_sprites++] = *sprite;
            if(gb.num_scanline_sprites == MAX_SCANLINE_SPRITES)
                break;
        }
    }
}

static void pixel_transfer(void)
{
    if(gb.lcd->control.bg_and_window_enable)
    {
        uint8_t tile_mode = gb.lcd->control.bg_and_window_tile_data_area;
        tile_t *tiles = (tile_t *)(gb.memory + (tile_mode ? 0x8000 : 0x9000));
        uint8_t bg_tilemap_mode = gb.lcd->control.bg_tile_map_area;
        uint8_t *bg_tilemap = (gb.memory + (bg_tilemap_mode ? 0x9C00 : 0x9800));
        uint8_t start = (gb.lcd->scx/8)%32;
        uint8_t end = (start + 21)%32;
        int16_t x = -(gb.lcd->scx%8);
        for(uint8_t i = start; i != end; i++)
        {
            int16_t y = (gb.lcd->scy + gb.lcd->ly);
            uint8_t id = bg_tilemap[32*((y/8)%32) + (i%32)];
            tile_t *tile = &tiles[tile_mode ? id : (int8_t)id];
            draw_tile_on_scanline(x, gb.lcd->ly, tile->lines[y%8], gb.lcd->bgp, (draw_flags_t){ 0 });
            x += 8;
        }
        if(gb.lcd->control.window_enable && gb.lcd->ly >= gb.lcd->wy)
        {
            uint8_t window_tilemap_mode = gb.lcd->control.window_tile_map_area;
            uint8_t *window_tilemap = (gb.memory + (window_tilemap_mode ? 0x9C00 : 0x9800));
            x = 0;
            for(uint8_t i = 0; i != 21; i++)
            {
                int16_t y = (gb.lcd->ly - gb.lcd->wy);
                uint8_t id = window_tilemap[32*(y/8) + (x/8)%32];
                tile_t *tile = &tiles[tile_mode ? id : (int8_t)id];
                draw_tile_on_scanline(((gb.lcd->wx - 7) + x), gb.lcd->ly, tile->lines[y%8], gb.lcd->bgp, (draw_flags_t){ 0 });
                x += 8;
            }
        }
    }
    if(gb.lcd->control.obj_enable)
    {
        tile_t *tiles = (tile_t *)(gb.memory + 0x8000);
        palette_t palettes[] = { gb.lcd->obp0, gb.lcd->obp1 };
        for(uint8_t i = 0; i < gb.num_scanline_sprites; i++)
        {
            sprite_attribute_t *sprite = &gb.scanline_sprites[i];
            int16_t x = (sprite->px - 8);
            int16_t y = (sprite->py - 16);
            uint8_t id = sprite->tile;
            if(gb.lcd->control.obj_size)
            {
                if((gb.lcd->ly - y) <= 7)
                    id = (sprite->flags.flipy ? (sprite->tile + 1) : sprite->tile);
                else
                    id = (sprite->flags.flipy ? sprite->tile : (sprite->tile + 1));
            }
            uint8_t line_idx = (gb.lcd->ly - y)%8;
            if(sprite->flags.flipy)
                line_idx = (7 - line_idx);
            palette_t palette = palettes[sprite->flags.palette];
            draw_flags_t flags =
            {
                .transparency = 1,
                .flip = sprite->flags.flipx,
                .prio_bg = sprite->flags.bg_and_window,
            };
            draw_tile_on_scanline(x, gb.lcd->ly, tiles[id].lines[line_idx], palette, flags);
        }
    }
}

static LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch(msg)
    {
        case WM_CLOSE:
        {
            save();
            DestroyWindow(window);
            PostQuitMessage(0);
            break;
        }
        case WM_COMMAND:
        {
            switch(LOWORD(wparam))
            {
                case MENU_OPEN:
                {
                    char path[MAX_PATH] = { 0 };
                    OPENFILENAME open_file =
                    {
                        .lStructSize = sizeof(OPENFILENAME),
                        .hwndOwner = window,
                        .lpstrFile = path,
                        .nMaxFile = sizeof(path),
                        .lpstrFilter = "Rom Files (*.gb)\0*.gb\0",
                        .nFilterIndex = 1,
                        .Flags = (OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR),
                    };
                    if(GetOpenFileName(&open_file))
                    {
                        save();
                        load(path);
                    }
                    break;
                }
                case MENU_RESET:
                {
                    save();
                    reset();
                    break;
                }
                case MENU_QUIT:
                {
                    SendMessage(window, WM_CLOSE, 0, 0);
                    break;
                }
            }
            break;
        }
        default:
        {
            result = DefWindowProc(window, msg, wparam, lparam);
            break;
        }
    }
    return(result);
}

int main(void)
{
    gb = (gameboy_t)
    {
        .memory = VirtualAlloc(NULL, 0x10000, (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE),
        .framebuffer = VirtualAlloc(NULL, sizeof(uint32_t)*SCREEN_W*SCREEN_H, (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE),
        .rom = VirtualAlloc(NULL, MAX_ROM_SIZE, (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE),
        .ram = VirtualAlloc(NULL, MAX_RAM_SIZE, (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE),
    };

    gb.cartridge_header = (cartridge_header_t *)(gb.memory + 0x100);
    gb.joypad = (joypad_t *)(gb.memory + 0xFF00);
    gb.timer = (timer_t *)(gb.memory + 0xFF04);
    gb.lcd = (lcd_t *)(gb.memory + 0xFF40),
    gb.interrupt_e = (interrupt_t *)(gb.memory + 0xFFFF);
    gb.interrupt_f = (interrupt_t *)(gb.memory + 0xFF0F);

    for(uint32_t i = 0; i < MAX_ROM_SIZE/0x4000; i++)
        gb.rom_banks[i] = gb.rom + i*0x4000;
    for(uint32_t i = 0; i < MAX_RAM_SIZE/0x2000; i++)
        gb.ram_banks[i] = gb.ram + i*0x2000;

    reset();

    WNDCLASS window_class =
    {
        .style = (CS_HREDRAW | CS_VREDRAW | CS_OWNDC),
        .lpfnWndProc = window_callback,
        .hInstance = GetModuleHandle(NULL),
        .hIcon = LoadIcon(NULL, IDI_APPLICATION),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = GetStockObject(NULL_BRUSH),
        .lpszClassName = "window_class",
    };

    if(RegisterClass(&window_class))
    {
        LONG window_w = SCREEN_SCALE*SCREEN_W;
        LONG window_h = SCREEN_SCALE*SCREEN_H;
        RECT r = { .right = window_w, .bottom = window_h };
        DWORD window_style = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
        AdjustWindowRectEx(&r, window_style, TRUE, 0);
        LONG w = (r.right - r.left);
        LONG h = (r.bottom - r.top);
        HWND window = CreateWindowEx(0, window_class.lpszClassName, "tiny_gb", window_style, CW_USEDEFAULT, CW_USEDEFAULT, w, h, NULL, NULL, window_class.hInstance, NULL);

        if(window)
        {
            HMENU menu = CreateMenu();
            AppendMenu(menu, MF_STRING, MENU_OPEN, "Open...");
            AppendMenu(menu, MF_STRING, MENU_RESET, "Reset");
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
            AppendMenu(menu, MF_STRING, MENU_QUIT, "Quit");

            HMENU menubar = CreateMenu();
            AppendMenu(menubar, MF_POPUP, (UINT_PTR)menu, "File");
            SetMenu(window, menubar);

            ShowWindow(window, SW_SHOWNORMAL);
            HDC context = GetDC(window);

            BITMAPINFO bmpi =
            {
                .bmiHeader.biSize = sizeof(BITMAPINFOHEADER),
                .bmiHeader.biWidth = SCREEN_W,
                .bmiHeader.biHeight = -SCREEN_H,
                .bmiHeader.biPlanes = 1,
                .bmiHeader.biBitCount = 32,
                .bmiHeader.biCompression = BI_RGB,
            };

            uint64_t gb_tick = 1000000000/CLOCK_FREQUENCY;
            uint64_t accumulator = 0;

            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);
            LARGE_INTEGER ticks;
            QueryPerformanceCounter(&ticks);

            bool running = true;
            while(running)
            {
                MSG msg;
                while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
                {
                    running = (msg.message != WM_QUIT);
                    if(!running)
                        break;
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                LARGE_INTEGER old_ticks = ticks;
                QueryPerformanceCounter(&ticks);
                accumulator += min((1000000000*(ticks.QuadPart - old_ticks.QuadPart))/frequency.QuadPart, 100000000);

                while(accumulator >= (gb.op_cycles*gb_tick))
                {
                    accumulator -= (gb.op_cycles*gb_tick);
                    gb.op_cycles = 0;

                    check_interrupt();
                    uint8_t op = mem_r(gb.registers.pc++);
                    execute_op(op);

                    if(gb.state.dma_transfer)
                    {
                        gb.cycles.dma += gb.op_cycles;
                        if(gb.cycles.dma >= 160)
                        {
                            uint16_t address = COMBINE(gb.memory[0xFF46], 00);
                            memcpy(gb.memory + 0xFE00, gb.memory + address, 0xA0);
                            gb.state.dma_transfer = 0;
                        }
                    }

                    gb.cycles.div += gb.op_cycles;
                    if(!gb.state.stop && gb.cycles.div >= 256)
                    {
                        gb.timer->div += 1;
                        gb.cycles.div -= 256;
                    }

                    if(gb.timer->control.enable)
                    {
                        uint16_t clock_cycles = 0;
                        switch(gb.timer->control.clock)
                        {
                            case 0: clock_cycles = 1024; break;
                            case 1: clock_cycles = 16; break;
                            case 2: clock_cycles = 64; break;
                            case 3: clock_cycles = 256; break;
                        }
                        gb.cycles.tac += gb.op_cycles;
                        if(gb.cycles.tac >= clock_cycles)
                        {
                            if(gb.timer->counter == 0xFF)
                            {
                                gb.timer->counter = gb.timer->modulo;
                                gb.interrupt_f->timer = 1;
                            }
                            else
                            {
                                gb.timer->counter += 1;
                            }
                            gb.cycles.tac -= clock_cycles;
                        }
                    }

                    if(gb.lcd->control.enable)
                    {
                        gb.cycles.dots += gb.op_cycles;

                        switch(gb.lcd->status.mode)
                        {
                            case LCD_MODE_SCAN_OAM:
                            {
                                if(gb.cycles.dots >= 80)
                                {
                                    scan_oam();
                                    set_mode(LCD_MODE_PIXEL_TRANSFER);
                                    gb.cycles.dots -= 80;
                                }
                                break;
                            }
                            case LCD_MODE_PIXEL_TRANSFER:
                            {
                                if(gb.cycles.dots >= 172)
                                {
                                    pixel_transfer();
                                    set_mode(LCD_MODE_HBLANK);
                                    gb.cycles.dots -= 172;
                                }
                                break;
                            }
                            case LCD_MODE_HBLANK:
                            {
                                if(gb.cycles.dots >= 204)
                                {
                                    set_ly(gb.lcd->ly + 1);
                                    if(gb.lcd->ly == 144)
                                    {
                                        StretchDIBits(context, 0, 0, window_w, window_h, 0, 0, SCREEN_W, SCREEN_H, gb.framebuffer, &bmpi, DIB_RGB_COLORS, SRCCOPY);
                                        set_mode(LCD_MODE_VBLANK);
                                    }
                                    else
                                    {
                                        set_mode(LCD_MODE_SCAN_OAM);
                                    }
                                    gb.cycles.dots -= 204;
                                }
                                break;
                            }
                            case LCD_MODE_VBLANK:
                            {
                                if(gb.cycles.dots >= 456)
                                {
                                    set_ly(gb.lcd->ly + 1);
                                    if(gb.lcd->ly == 154)
                                    {
                                        set_mode(LCD_MODE_SCAN_OAM);
                                        set_ly(0);
                                    }
                                    gb.cycles.dots -= 456;
                                }
                                break;
                            }
                        }
                    }
                }
                SleepEx(1, false);
            }
        }
    }

    return(0);
}
