#include "Logger.h"
#include "StringUtils.h"
#include "Exceptions.h"
#include "Constants.h"

#include <distorm.h>
#include <mnemonics.h>
#include <windows.h>

using namespace std;


#define LOG_FILE "dump.log"

const uint8_t breakpointConstant = 0xCC;

int64_t getRegVal ( uint8_t type, const CONTEXT& context );


void processInstruction ( uint8_t buffer[64], const _CodeInfo& ci, const _DInst& di, const CONTEXT& context )
{
    // int numMemOperands = 0;
    // uint32_t addr = 0;

    // for ( const auto& op : di.ops )
    // {
    //     switch ( op.type )
    //     {
    //         case O_NONE: // operand is to be ignored.
    //             continue;

    //         case O_DISP: // memory dereference with displacement only, instruction.disp.
    //             addr = getRegVal ( di.segment, context ) + di.disp;
    //             ++numMemOperands;
    //             break;

    //         case O_SMEM: // simple memory dereference with optional displacement (a single register memory dereference).
    //             addr = getRegVal ( di.segment, context ) + getRegVal ( op.index, context ) + di.disp;
    //             ++numMemOperands;
    //             break;

    //         case O_MEM:  // complex memory dereference (optional fields: s/i/b/disp).
    //             addr = getRegVal ( di.segment, context ) + getRegVal ( di.base, context )
    //                    + getRegVal ( op.index, context ) * di.scale + di.disp;
    //             ++numMemOperands;
    //             break;

    //         case O_REG:  // index holds global register index.
    //         case O_IMM:  // instruction.imm.
    //         case O_IMM1: // instruction.imm.ex.i1.
    //         case O_IMM2: // instruction.imm.ex.i2.
    //         case O_PC:   // the relative address of a branch instruction (instruction.imm.addr).
    //         case O_PTR:  // the absolute target address of a far branch instruction (instruction.imm.ptr.seg/off).
    //             break;

    //         default:
    //             PRINT ( "Unhandled operand type: %d", ( int ) op.type );
    //             exit ( -1 );
    //     }
    // }

    // if ( numMemOperands == 0 )
    //     return;

    _DecodedInst decodedInst;
    distorm_format ( &ci, &di, &decodedInst );

    LOG ( "%08X: %s:\t%s%s%s",
          ( uint32_t ) context.Eip,
          toBase64 ( &buffer[decodedInst.offset], decodedInst.size ),
          lowerCase ( ( char * ) decodedInst.mnemonic.p ),
          ( decodedInst.operands.length != 0 ? " " : "" ),
          lowerCase ( ( char * ) decodedInst.operands.p ) );

    // LOG ( "addr = %08X", addr );
}

int main ( int argc, char *argv[] )
{
    Logger::get().initialize ( LOG_FILE, LOG_LOCAL_TIME );

    string exe = "MBAA.exe";
    string cwd;

    if ( argc > 1 )
    {
        exe = argv[1];
        cwd = exe.substr ( 0, exe.find_last_of ( "/\\" ) );
    }

    HANDLE process = 0;
    HANDLE thread = 0;

    // Start process
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory ( &si, sizeof ( si ) );
    ZeroMemory ( &pi, sizeof ( pi ) );
    si.cb = sizeof ( si );

    if ( !CreateProcess ( 0, const_cast<char *> ( exe.c_str() ), 0, 0, TRUE, DEBUG_ONLY_THIS_PROCESS, 0,
                          cwd.empty() ? 0 : cwd.c_str(), &si, &pi ) )
    {
        PRINT ( "CreateProcess failed: %s", WinException::getLastError() );
        exit ( -1 );
    }

    process = pi.hProcess;
    // thread = pi.hThread;

    uint8_t buffer[64];
    DEBUG_EVENT event;
    CONTEXT context;
    ZeroMemory ( &buffer, sizeof ( buffer ) );
    ZeroMemory ( &event, sizeof ( event ) );
    ZeroMemory ( &context, sizeof ( context ) );

    bool stepping = false;

    // Wait for debugging events
    for ( bool debugging = true; debugging; )
    {
        if ( !WaitForDebugEvent ( &event, INFINITE ) )
        {
            PRINT ( "WaitForDebugEvent failed: %s", WinException::getLastError() );
            exit ( -1 );
        }

        switch ( event.dwDebugEventCode )
        {
            case CREATE_PROCESS_DEBUG_EVENT:
                PRINT ( "CREATE_PROCESS_DEBUG_EVENT" );
                // Breakpoint at the start of the main loop
                WriteProcessMemory ( process, CC_LOOP_START_ADDR, &breakpointConstant, 1, 0 );
                FlushInstructionCache ( process, CC_LOOP_START_ADDR, 1 );
                break;

            case EXIT_PROCESS_DEBUG_EVENT:
                PRINT ( "EXIT_PROCESS_DEBUG_EVENT" );
                debugging = false;
                break;

            case EXCEPTION_SINGLE_STEP:
                PRINT ( "EXCEPTION_SINGLE_STEP" );
                break;

            case EXCEPTION_DEBUG_EVENT:
                switch ( event.u.Exception.ExceptionRecord.ExceptionCode )
                {
                    case EXCEPTION_BREAKPOINT:
                        if ( !thread )
                        {
                            if ( ! ( thread = OpenThread ( THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                                           0, event.dwThreadId ) ) )
                            {
                                PRINT ( "OpenThread failed: %s", WinException::getLastError() );
                                exit ( -1 );
                            }
                        }

                        context.ContextFlags = CONTEXT_ALL;
                        GetThreadContext ( thread, &context );

                        // Reset the breakpoint and start stepping
                        if ( !stepping && context.Eip == int ( CC_LOOP_START_ADDR + 1 ) )
                        {
                            --context.Eip;
                            context.EFlags |= 0x100;
                            SetThreadContext ( thread, &context );

                            uint8_t original = 0x6A;
                            WriteProcessMemory ( process, CC_LOOP_START_ADDR, &original, 1, 0 );
                            FlushInstructionCache ( process, CC_LOOP_START_ADDR, 1 );

                            stepping = true;

                            PRINT ( "threadId = %u", ( uint32_t ) event.dwThreadId );
                        }

                        break;

                    case EXCEPTION_SINGLE_STEP:
                    {
                        // LOG ( "eip = %08X", ( uint32_t ) context.Eip );

                        ReadProcessMemory ( process, ( char * ) context.Eip, buffer, sizeof ( buffer ), 0 );
                        // WriteProcessMemory ( process, ( char * ) context.Eip, buffer, 1, 0 );
                        // FlushInstructionCache ( process, ( char * ) context.Eip, 1 );

                        _DInst di;
                        unsigned int diCount = 0;

                        _CodeInfo ci;
                        ci.code = buffer;
                        ci.codeLen = sizeof ( buffer );
                        ci.codeOffset = 0;
                        ci.dt = Decode32Bits;
                        ci.features = DF_NONE;

                        // Disassemble just the first instruction in the buffer
                        if ( distorm_decompose ( &ci, &di, 1, &diCount ) == DECRES_INPUTERR
                                || diCount == 0 || di.flags == FLAG_NOT_DECODABLE )
                        {
                            PRINT ( "decode failed @ eip = %08X", ( uint32_t ) context.Eip );
                            exit ( -1 );
                        }

                        processInstruction ( buffer, ci, di, context );

                        // --context.Eip;
                        // context.EFlags |= 0x100;
                        // SetThreadContext ( thread, &context );
                        break;
                    }

                    default:
                        // PRINT ( "Unexpected ExceptionCode: %d", event.u.Exception.ExceptionRecord.ExceptionCode );
                        // exit ( -1 );
                        break;
                }

                break;

            // Ignored events
            case CREATE_THREAD_DEBUG_EVENT:
            case EXIT_THREAD_DEBUG_EVENT:
            case LOAD_DLL_DEBUG_EVENT:
            case UNLOAD_DLL_DEBUG_EVENT:
                // PRINT ( "Unhandled dwDebugEventCode: %d", event.dwDebugEventCode );
                break;

            default:
                PRINT ( "Unexpected dwDebugEventCode: %d", event.dwDebugEventCode );
                exit ( -1 );
        }

        if ( !ContinueDebugEvent ( event.dwProcessId, event.dwThreadId, DBG_CONTINUE ) )
        {
            PRINT ( "ContinueDebugEvent failed: %s", WinException::getLastError() );
            exit ( -1 );
        }
    }

    CloseHandle ( thread );

    Logger::get().deinitialize();
    return 0;
}

int64_t getRegVal ( uint8_t type, const CONTEXT& context )
{
    if ( SEGMENT_IS_DEFAULT ( type ) )
        return 0;

    switch ( type )
    {
        case R_NONE:
            return 0;

        case R_AL:
            return * ( ( int8_t * ) &context.Eax );
        case R_BL:
            return * ( ( int8_t * ) &context.Ebx );
        case R_CL:
            return * ( ( int8_t * ) &context.Ecx );
        case R_DL:
            return * ( ( int8_t * ) &context.Edx );

        case R_AH:
            return * ( ( ( int8_t * ) &context.Eax ) + 1 );
        case R_BH:
            return * ( ( ( int8_t * ) &context.Ebx ) + 1 );
        case R_CH:
            return * ( ( ( int8_t * ) &context.Ecx ) + 1 );
        case R_DH:
            return * ( ( ( int8_t * ) &context.Edx ) + 1 );

        case R_AX:
            return * ( ( ( int16_t * ) &context.Eax ) + 1 );
        case R_BX:
            return * ( ( ( int16_t * ) &context.Ebx ) + 1 );
        case R_CX:
            return * ( ( ( int16_t * ) &context.Ecx ) + 1 );
        case R_DX:
            return * ( ( ( int16_t * ) &context.Edx ) + 1 );

        case R_EAX:
            return context.Eax;
        case R_EBX:
            return context.Ebx;
        case R_ECX:
            return context.Ecx;
        case R_EDX:
            return context.Edx;

        case R_ESP:
            return context.Esp;
        case R_EBP:
            return context.Ebp;
        case R_ESI:
            return context.Esi;
        case R_EDI:
            return context.Edi;

        case R_ES:
            return context.SegEs * 0x10;
        case R_CS:
            return context.SegCs * 0x10;
        case R_SS:
            return context.SegSs * 0x10;
        case R_DS:
            return context.SegDs * 0x10;
        case R_FS:
            return context.SegFs * 0x10;
        case R_GS:
            return context.SegGs * 0x10;

        default:
            PRINT ( "Unhandled register type: %d", ( int ) type );
            exit ( -1 );
    }
}
