#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <DbgHelp.h>
#include <thread>
#include <chrono>

//
// Link required libraries.
//
#pragma comment( lib, "Dbghelp.lib" )

//
// Define gepard module.
//
HMODULE module_game   = nullptr;
HMODULE module_gepard = nullptr;

//
// Define client version.
//
#define CLIENT_VERSION 2018

namespace Ragnarok
{
	template<typename T>
	struct LINKED_LIST
	{
		T* Next;
		T* Prev;
	};

	struct ListIterMemFileAndPak : public LINKED_LIST<struct ListIterMemFileAndPak>
	{
		class CMemFile* MemFile;
		class CGPak*    Pak;
	};

	class CFileMgr
	{
	public:
		LINKED_LIST<ListIterMemFileAndPak> m_pakList;
	};

	//
	// Validate structure.
	//
	static_assert( sizeof( CFileMgr ) == 0x08, "0x08 != sizeof( CFileMgr )" );

	class CMemFile
	{
	public:
		/* this+ 0 */ DWORD  m_vTable;
		/* this+ 4 */ HANDLE m_hFile;
		/* this+ 8 */ HANDLE m_hFileMap;
		/* this+12 */ DWORD m_dwFileSize;
		/* this+16 */ DWORD m_dwOpenOffset;
		/* this+20 */ DWORD m_dwOpenSize;
		/* this+24 */ DWORD m_dwFileMappingSize;
		/* this+28 */ DWORD m_dwAllocationGranuarity; // mask
		/* this+32 */ const BYTE* m_pFile;
		/* this+36 */ std::vector<BYTE> m_pFileBuf;
	}; //Size=0x004C

	//
	// Hash structure.
	//
	class CHash
	{
	public:
		DWORD m_HashCode;
		char  m_String[ 252 ];
	};

	//
	// Validate structure.
	//
	static_assert( sizeof( CHash ) == 0x0100, "0x0100 != sizeof( CHash )" );

	//
	// PakPack structure.
	//
	struct PakPack
	{
		/* this+ 0 */ DWORD m_dataSize;
		/* this+ 4 */ DWORD m_compressSize;
		/* this+ 8 */ DWORD m_size;
		/* this+12 */ DWORD m_Offset;
		/* this+16 */ char m_type;
		/* this+20 */ CHash m_fName;
	};

	//
	// Validate structure.
	//
	static_assert( 0x0014 == offsetof( PakPack, m_fName ), "0x0014 != offsetof( PakPack, m_fName )" );
	static_assert( 0x0114 == sizeof( PakPack ),            "0x0114 != sizeof( PakPack )" );

	//
	// Pak structure.
	//
	class CGPak
	{
	public:
		DWORD                m_vTable;
		__int32              m_FileVer;
		__int32              m_FileCount;
		__int32              m_PakInfoOffset;
		__int32              m_PakInfoSize;
#if CLIENT_VERSION <= 2008
		__int32              m_Unknown;
#endif
		std::vector<PakPack> m_PakPack;
		std::vector<BYTE>    m_pDecBuf;
		CMemFile* m_memFile;
		char                 pad_0x0030[ 0x18 ];
	};

#if CLIENT_VERSION <= 2008
	static_assert( 0x0018 == offsetof( CGPak, m_PakPack ), "0x0018 != offsetof( CGPak, m_memFile )" );
#else
	static_assert( 0x0014 == offsetof( CGPak, m_PakPack ), "0x0018 != offsetof( CGPak, m_memFile )" );
#endif
}

//
// Find pattern.
//
char* FindPattern( HMODULE module, const char* signature, const char* mask )
{
	//
	// Get module nt headers.
	//
	IMAGE_NT_HEADERS* nt_headers = ImageNtHeader( module );
	
	//
	// Check if nt headers is valid.
	//
	if ( !nt_headers ) return nullptr;

	//
	// Get module size.
	//
	auto module_size = nt_headers->OptionalHeader.SizeOfImage;

	//
	// Get signature length.
	//
	auto signature_length = strlen( mask );

	//
	// Iterate module.
	//
	for ( size_t i = 0u; i < module_size - signature_length; i++ )
	{
		//
		// Check if signature is found.
		//
		bool found = true;

		//
		// Iterate signature.
		//
		for ( size_t j = 0u; j < signature_length; j++ )
		{
			//
			// Check if signature is found.
			//
			if ( mask[ j ] != '?' && signature[ j ] != reinterpret_cast<char*>( module )[ i + j ] )
			{
				//
				// Signature is not found.
				//
				found = false;
				break;
			}
		}

		//
		// Check if signature is found.
		//
		if ( found )
			return reinterpret_cast< char* >( module ) + i;
	}

	//
	// Return nullptr.
	//
	return nullptr;
}

//
// Export gepard 2.0 variables.
//
EXTERN_C _declspec( dllexport ) int gepard_1 = 0;
EXTERN_C _declspec( dllexport ) int gepard_2 = 0;
EXTERN_C _declspec( dllexport ) int gepard_3 = 0;

//
// Export gepard 3.0 function.
//
EXTERN_C _declspec( dllexport ) int get_gepard_version( )
{
	//
	// Check if this feature is already initialized.
	//
	if ( !module_gepard ) return NULL;

	//
	// Get original function `get_gepard_version`.
	//
	const auto original_function = GetProcAddress( module_gepard, "get_gepard_version" );

	//
	// Execute original function `get_gepard_version`.
	//
	return original_function ? reinterpret_cast< int( * )( ) >( original_function )( ) : NULL;
}

//
// Main thread.
//
void main_thread( )
{
	//
	// Wait for game module.
	//
	while ( !module_game ) Sleep( 100 );

	//
	// Wait for gepard module.
	//
	while ( !module_gepard ) Sleep( 100 );

	//
	//
	//
	char* ptr_file_mgr = nullptr;
	char* ptr_get_data = nullptr;

	//
	// Get ragnarok: CGPak::GetData
	//
	if ( const auto pattern_result = FindPattern( module_game, "\xE8\x00\x00\x00\x00\x84\xC0\x75\x00\x68\x00\x80\x00\x00\x6A", "x????xxx?xxxxxx" ) )
		ptr_get_data = pattern_result + *reinterpret_cast< int* >( pattern_result + 1 ) + 5;
	else
		MessageBoxA( nullptr, "Failed to find Ragnarok::CGPak::GetData", "Error", MB_OK | MB_ICONERROR );

	//
	// Get ragnarok: g_fileMgr
	//
	if ( const auto pattern_result = FindPattern( module_game, "\x8B\x15\x00\x00\x00\x00\x8B\x4E\x00\x85\xC9", "xx????xx?xx" ) )
		ptr_file_mgr = *reinterpret_cast< char** >( pattern_result + 2 );
	else if ( const auto pattern_result = FindPattern( module_game, "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xB8\x00\x00\x00\x00\xA8\x11", "x????x????x????xx" ) )
		ptr_file_mgr = *reinterpret_cast< char** >( pattern_result + 1 );
	else if ( const auto pattern_result = FindPattern( module_game, "\xB9\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x7D\x00\x85\xC0", "x????x????xx?xx" ) )
		ptr_file_mgr = *reinterpret_cast< char** >( pattern_result + 1 ) + 4;
	else
		MessageBoxA( nullptr, "Failed to find Ragnarok::CFileMgr", "Error", MB_OK | MB_ICONERROR );

	//
	// Main loop.
	//
	while ( ptr_file_mgr && ptr_get_data )
	{
		//
		// 
		//
		if ( GetAsyncKeyState( VK_F1 ) & 1 )
		{
			//
			// Get file manager.
			//
			#if CLIENT_VERSION <= 2018
			auto file_mgr = *reinterpret_cast< Ragnarok::CFileMgr** >( ptr_file_mgr );
			#else
			auto file_mgr = **reinterpret_cast< Ragnarok::CFileMgr*** >( ptr_file_mgr );
			#endif

			//
			// Iterate all pak files.
			//
			for ( auto iter = file_mgr->m_pakList.Next; iter != file_mgr->m_pakList.Prev; iter = iter->Next )
			{
				//
				// Get file name.
				//
				char name[ MAX_PATH ] = { 0 };
				GetFinalPathNameByHandleA( iter->MemFile->m_hFile, name, MAX_PATH, FILE_NAME_NORMALIZED );

				//
				// Ignore pak with no files.
				//
				if ( iter->Pak->m_FileCount == 0 ) continue;

				//
				// Ask user to dump pak.
				//
				if ( MessageBoxA( nullptr, name, "Should dump?", MB_YESNO ) != IDYES ) continue;

				//
				// Iterate all pak pack files.
				//
				for ( const auto& pak_pack : iter->Pak->m_PakPack )
				{
					//
					// Allocate file buffer.
					//
					auto file_buffer = std::make_unique<char[ ]>( pak_pack.m_size + 0x0008 /* Dont ask me why but game does */ );

					//
					// Ensure file buffer was allocated correctly.
					//
					if ( !file_buffer ) continue;

					//
					// Dump file.
					//
					auto file_result = reinterpret_cast< bool( __thiscall* )( Ragnarok::CGPak*, const Ragnarok::PakPack*, void* buffer ) >( ptr_get_data )(
						iter->Pak, &pak_pack, file_buffer.get( )
					);

					//
					// Ensure file was dumped correctly.
					//
					if ( !file_result ) continue;

					//
					// Parse file name.
					//
					auto file_name = std::string( "DumpFiles\\" + std::string( pak_pack.m_fName.m_String ) );
					auto file_path = std::filesystem::path( file_name );
					auto file_path_parent = file_path.parent_path( );
					auto file_path_filename = file_path.filename( );

					//
					// Create required directories.
					//
					try
					{
						std::filesystem::create_directories( file_path_parent );
					}
					catch ( const std::exception& )
					{
						MessageBoxA( nullptr, file_name.c_str( ), "Failed to create directories", MB_OK | MB_ICONERROR );
						continue;
					}

					//
					// Save file.
					//
					std::ofstream file( file_name, std::ios::out | std::ios::binary );
					file.write( file_buffer.get( ), pak_pack.m_size );
					file.close( );
				}
			}

			//
			// Notify user.
			//
			MessageBoxA( nullptr, "Process completed", "Information", MB_OK | MB_ICONINFORMATION );
		}

		//
		// Avoid high CPU usage.
		//
		std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
	}
}

BOOL APIENTRY DllMain( HMODULE module, DWORD reason, LPVOID reserved )
{
	//
	// Check if this is a process attach.
	//
	if ( reason == DLL_PROCESS_ATTACH )
	{
		//
		// Get game module.
		//
		module_game = GetModuleHandle( nullptr );

		//
		// Get gepard module.
		//
		module_gepard = LoadLibrary( TEXT( "gepard.bin" ) );

		//
		// Copy gepard variables.
		//
		if ( module_gepard and !GetProcAddress( module_gepard, "get_gepard_version" ) )
		{
			gepard_1 = *reinterpret_cast< int* >( GetProcAddress( module_gepard, reinterpret_cast< LPCSTR >( 1 ) ) );
			gepard_2 = *reinterpret_cast< int* >( GetProcAddress( module_gepard, reinterpret_cast< LPCSTR >( 2 ) ) );
			gepard_3 = *reinterpret_cast< int* >( GetProcAddress( module_gepard, reinterpret_cast< LPCSTR >( 3 ) ) );
		}

		//
		// Initialize main thread.
		//
		CreateThread( nullptr, 0, reinterpret_cast< LPTHREAD_START_ROUTINE >( &main_thread ), nullptr, 0, nullptr );
	}

	return TRUE;
}