#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Define type
typedef unsigned short u16;
typedef unsigned char u8;

const int SCREEN_WIDTH = 64 * 20;
const int SCREEN_HEIGHT = 32 * 20;

// Global variables
//---------- Graphic -----------------
SDL_Window* window = NULL;
SDL_Surface* screenSurface = NULL;
SDL_Surface* image = NULL;
//---------- CPU ---------------------
u8 memory[4096]; // 0x1000
u8 V[16]; // general purpose register
u16 I; // Index register
u16 PC; // Program counter
u8 gfx[64 * 32];
u8 delay_timer;
u8 sound_timer;
u16 stack[16];
u16 SP; // stack pointer
u8 key[16];
u16 opcode;

u8 chip8_fontset[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

void cpu_init(){
    PC = 0x200;
    opcode = 0;
    I = 0;
    SP = 0;
    // Clear display
    memset(gfx, 0, sizeof(gfx));
    // Clear stack
    memset(stack, 0, sizeof(stack));
    // Clear general purpose registers
    memset(V, 0, sizeof(V));
    // Clear memory
    memset(memory, 0, sizeof(memory));
    // Load font set
    for(u8 i = 0; i < 80; ++i){
        memory[i] = chip8_fontset[i];
    }
    // Reset timer
    delay_timer = 0;
    sound_timer = 0;
}

void play_beep(){
}

void cycle(){
    u16 addr, xx, yy, id;
    u8 x, y, value, tmp, w, h;
    // Fetch opcode
    opcode = memory[PC] << 8 | memory[PC + 1];
    // Decode Opcode & Execute Opcode
    if(opcode == 0x0){
        return;
    }
    printf("PC = %d; opcode = %x\n", PC, opcode);
    switch((opcode & 0xF000) >> 12){
        case 0x0:
            if((opcode & 0x0F) == 0){
                //00E0 	Clears the screen
                memset(gfx, 0, sizeof(gfx));
                PC += 2;
            }else{
                //00EE 	Returns from a subroutine
                --SP;
                PC = stack[SP];
            }
            break;
        case 0x1:
            // 1NNN 	Jumps to address NNN
            addr = (opcode & 0x0FFF);
            PC = addr;
            break;
        case 0x2:
            //2NNN 	Calls subroutine at NNN
            stack[SP] = PC + 2;
            ++SP;
            addr = (opcode & 0x0FFF);
            PC = addr;
            break;
        case 0x3:
            //3XNN 	Skips the next instruction if VX equals NN
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0xFF;
            if(V[x] == value){
                PC += 4;
            }else{
                PC += 2;
            }
            break;
        case 0x4:
            //4XNN 	Skips the next instruction if VX doesn't equal NN
            x = (opcode & 0x0F00) >> 8;
            value = opcode & 0xFF;
            if(V[x] != value){
                PC += 4;
            }else{
                PC += 2;
            }
            break;
        case 0x5:
            //5XY0 	Skips the next instruction if VX equals VY.
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0xF0) >> 4;
            if(V[x] == V[y]){
                PC += 4;
            }else{
                PC += 2;
            }
            break;
        case 0x6:
            //6XNN 	Sets VX to NN.
            x = (opcode & 0x0F00) >> 8;
            value = (opcode & 0xFF);
            V[x] = value;
            PC += 2;
            break;
        case 0x7:
            //7XNN 	Adds NN to VX.
            x = (opcode & 0x0F00) >> 8;
            value = (opcode & 0xFF);
            V[x] += value;
            PC += 2;
            break;
        case 0x8:
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0xF0) >> 4;
            switch(opcode & 0x0F){
                case 0x0:
                    //8XY0 	Sets VX to the value of VY
                    V[x] = V[y];
                    break;
                case 0x1:
                    //8XY1 	Sets VX to VX or VY
                    V[x] |= V[y];
                    break;
                case 0x2:
                    //8XY2 	Sets VX to VX and VY
                    V[x] &= V[y];
                    break;
                case 0x3:
                    //8XY3 	Sets VX to VX xor VY
                    V[x] ^= V[y];
                    break;
                case 0x4:
                    //8XY4 	Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
                    tmp = V[x] + V[y];
                    if(tmp >= V[x] && tmp >= V[y]){
                        V[0xF] = 0;
                    }else{
                        V[0xF] = 1;
                    }
                    V[x] = tmp;
                    break;
                case 0x5:
                    //8XY5 	VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
                    if(V[x] < V[y]){
                        V[0xF] = 1;
                        V[x] += 256 - V[y];
                    }else{
                        V[0xF] = 0;
                        V[x] -= V[y];
                    }
                    break;
                case 0x6:
                    //8XY6 	Shifts VX right by one. VF is set to the value of the least significant bit of VX before the shift
                    V[0xF] = V[x] & 0x1;
                    V[x] >>= 1;
                    break;
                case 0x7:
                    //8XY7 	Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
                    if(V[y] < V[x]){
                        V[0xF] = 1;
                        V[x] = V[y] + (256 - V[x]);
                    }else{
                        V[0xF] = 0;
                        V[x] = V[y] - V[x];
                    }
                    break;
                case 0xE:
                    //8XYE 	Shifts VX left by one. VF is set to the value of the most significant bit of VX before the shift
                    V[0xF] = (V[x] & 0x80) >> 7;
                    V[x] <<= 1;
                    break;
            }
            PC += 2;
            break;
        case 0x9:
            //9XY0 	Skips the next instruction if VX doesn't equal VY
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0xF0) >> 4;
            if(V[x] != V[y]){
                PC += 4;
            }else{
                PC += 2;
            }
        case 0xA:
            //ANNN 	Sets I to the address NNN.
            addr = opcode & 0x0FFF;
            I = addr;
            PC += 2;
            break;
        case 0xB:
            //BNNN 	Jumps to the address NNN plus V0
            addr = (opcode & 0x0FFF) + V[0];
            PC = addr;
            break;
        case 0xC:
            //CXNN 	Sets VX to the result of a bitwise and operation on a random number and NN
            x = (opcode & 0x0F00) >> 8;
            value = (opcode & 0xFF) & (rand() % 256);
            V[x] = value;
            PC += 2;
            break;
        case 0xD:
            //DXYN 	Sprites stored in memory at location in index register (I), 8bits wide. Wraps around the screen.
            //If when drawn, clears a pixel, register VF is set to 1 otherwise it is zero.
            //All drawing is XOR drawing (i.e. it toggles the screen pixels).
            //Sprites are drawn starting at position VX, VY. N is the number of 8bit rows that need to be drawn.
            //If N is greater than 1, second line continues at position VX, VY+1, and so on
            w = 8;
            h = opcode & 0x0F;
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0xF0) >> 4;
            for(u8 i = 0; i < h; ++i){
                u8 line = memory[I + i];
                for(u8 j = 0; j < w; ++j){
                    xx = x + j;
                    yy = y + i;
                    id = yy * 64 + xx;
                    if((line & (0x80 >> j)) != 0){
                        if(gfx[id] == 1){
                            V[0x0F] = 1;
                        }
                        gfx[id] ^= 1;
                    }
                }
            }
            PC += 2;
            break;
        case 0xE:
            x = (opcode & 0x0F00) >> 8;
            switch(opcode & 0xFF){
                case 0x9E:
                    //EX9E 	Skips the next instruction if the key stored in VX is pressed
                    if(key[V[x]] != 0){
                        PC += 4;
                    }else{
                        PC += 2;
                    }
                    break;
                case 0xA1:
                    //EXA1 	Skips the next instruction if the key stored in VX isn't pressed
                    if(key[V[x]] == 0){
                        PC += 4;
                    }else{
                        PC += 2;
                    }
            }
            break;
        case 0xF:
            x = (opcode & 0x0F00) >> 8;
            switch(opcode & 0xFF){
                case 0x07:
                    //FX07 	Sets VX to the value of the delay timer
                    V[x] = delay_timer;
                    break;
                case 0x0A:{
                    //FX0A 	A key press is awaited, and then stored in VX.
                    bool keyPressed = false;
                    for(xx = 0; xx < 16; ++xx){
                        if(key[xx] != 0){
                            keyPressed = true;
                            V[x] = xx;
                        }
                    }
                    if(!keyPressed){
                        return;
                    }
                }
                    break;
                case 0x15:
                    //FX15 	Sets the delay timer to VX.
                    delay_timer = V[x];
                    break;
                case 0x18:
                    //FX18 	Sets the sound timer to VX.
                    sound_timer = V[x];
                    break;
                case 0x1E:
                    //FX1E 	Adds VX to I
                    I += V[x];
                    if(I > 0xFFF){
                        V[0xF] = 1;
                    }else{
                        V[0xF] = 0;
                    }
                    break;
                case 0x29:
                    //FX29 	Sets I to the location of the sprite for the character in VX.
                    //Characters 0-F (in hexadecimal) are represented by a 4x5 font
                    I = V[x] * 5;
                    break;
                case 0x33:
                    //FX33 	Stores the binary-coded decimal representation of VX,
                    //with the most significant of three digits at the address in I,
                    //the middle digit at I plus 1, and the least significant digit at I plus 2.
                    //(In other words, take the decimal representation of VX,
                    //place the hundreds digit in memory at location in I, the tens digit at location I+1,
                    //and the ones digit at location I+2
                    memory[I] = V[x] / 100;
                    memory[I+1] = (V[x] % 100) / 10;
                    memory[I+2] = V[x] % 10;
                    break;
                case 0x55:
                    //FX55 	Stores V0 to VX (including VX) in memory starting at address I
                    for(xx = 0; xx <= x; ++xx){
                        memory[I + xx] = V[xx];
                    }
                    I += x + 1;
                    break;
                case 0x65:
                    //FX65 	Fills V0 to VX (including VX) with values from memory starting at address I
                    for(xx = 0; xx <= x; ++xx){
                        V[xx] = memory[I + xx];
                    }
                    I += x + 1;
                    break;
            }
            PC += 2;
            break;
    }
    // Update timers
    if(delay_timer > 0){
        --delay_timer;
    }

    if(sound_timer > 0){
        if(sound_timer == 1){
            play_beep();
        }
        --sound_timer;
    }
}

bool init() {
    bool success = true;
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf(" SDL could not initialize!\n Error: %s", SDL_GetError());
        success = false;
    }else{
        window = SDL_CreateWindow("Chip8Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if(window == NULL){
            printf("Cant create the window for emulator");
            success = false;
        }else{
            screenSurface = SDL_GetWindowSurface(window);
            //SDL_FillRect( screenSurface, NULL, SDL_MapRGB( screenSurface->format, 0xFF, 0xFF, 0xFF ) );

        }
    }
    return success;
}

bool loadMedia(){
    bool success = true;
    image = SDL_LoadBMP("duck.bmp");
    if(image == NULL){
        printf("Unable to load image. %s", SDL_GetError());
        success = false;
    }
    return success;
}

void loadRom(char path[]){
    FILE* input = fopen(path, "rb");
    if(input == NULL){
        printf("Cant open rom %s", path);
        return;
    }
    fseek(input, 0, SEEK_END);
    size_t fsize = ftell(input);
    rewind(input);

    char * buf = (char*) malloc(sizeof(char) * fsize);
    fread(buf, 1, fsize, input);
    for( int i = 0; i < fsize; ++i){
        memory[0x200 + i] = buf[i];
    }
    printf("Load rom %s successfully\n", path);
}

void close2(){
    SDL_FreeSurface(image);
    image = NULL;
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_Quit();
}
void drawScreen(){
    for(int i = 0; i < 64; ++i){
        for(int j = 0; j < 32; ++j){
            int id = j * 64 + i;
            SDL_Rect outlineRect = { i*20 , j*20, 20, 20};
            if(gfx[id] == 0){
                SDL_FillRect( screenSurface, &outlineRect, SDL_MapRGB( screenSurface->format, 0xFF, 0xFF, 0xFF ) );
            }else{
                SDL_FillRect( screenSurface, &outlineRect, SDL_MapRGB( screenSurface->format, 0xFF, 0, 0 ) );
            }
        }
    }
}

void debugRender()
{
	// Draw
	printf("-------------------------\n");
	for(int y = 0; y < 32; ++y)
	{
		for(int x = 0; x < 64; ++x)
		{
			if(gfx[(y*64) + x] == 0)
				printf("O");
			else
				printf(" ");
		}
		printf("\n");
	}
	printf("\n");
}

int main(int argc, char* argv[]){
    SDL_Event e;
    bool quit = false;
    init();
    cpu_init();
    loadMedia();
    loadRom("c8games/TANK");
    while(!quit){
        cycle();
        //debugRender();
        while(SDL_PollEvent(&e) != 0){
            if(e.type == SDL_QUIT){
                quit = true;
            }
            memset(key, 0, sizeof(key));
            if(e.type == SDL_KEYDOWN){
                switch(e.key.keysym.sym){
                    case SDLK_1:
                        key[0] = 1;
                        break;
                    case SDLK_2:
                        key[1] = 1;
                        break;
                    case SDLK_3:
                        key[2] = 1;
                        break;
                    case SDLK_4:
                        key[3] = 1;
                        break;
                    case SDLK_q:
                        key[4] = 1;
                        break;
                    case SDLK_w:
                        key[5] = 1;
                        break;
                    case SDLK_e:
                        key[6] = 1;
                        break;
                    case SDLK_r:
                        key[7] = 1;
                        break;
                    case SDLK_a:
                        key[8] = 1;
                        break;
                    case SDLK_s:
                        key[9] = 1;
                        break;
                    case SDLK_d:
                        key[10] = 1;
                        break;
                    case SDLK_f:
                        key[11] = 1;
                        break;
                    case SDLK_z:
                        key[12] = 1;
                        break;
                    case SDLK_x:
                        key[13] = 1;
                        break;
                    case SDLK_c:
                        key[14] = 1;
                        break;
                    case SDLK_v:
                        key[15] = 1;
                        break;
                }
            }
        }
        drawScreen();
        SDL_UpdateWindowSurface(window);
        SDL_Delay(50);
    }

    close2();
    return 0;
}
