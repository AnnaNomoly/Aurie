#include "object.hpp"

namespace Aurie
{
	AurieStatus ObCreateInterface(
		IN AurieModule* Module, 
		IN AurieInterfaceBase* Interface, 
		IN const char* InterfaceName
	)
	{
		if (ObInterfaceExists(InterfaceName))
			return AURIE_OBJECT_ALREADY_EXISTS;

		AurieInterfaceTableEntry table_entry = {};
		table_entry.Interface = Interface;
		table_entry.InterfaceName = InterfaceName;
		table_entry.OwnerModule = Module;

		// Make sure the interface knows it's being set up,
		// and that it succeeds at doing so. We don't want an
		// uninitialized, half-broken interface exposed!

		AurieStatus last_status = Interface->Create();
		if (!AurieSuccess(last_status))
			return last_status;

		return Internal::ObpAddInterfaceToTable(
			Module,
			table_entry
		);
	}

	bool ObInterfaceExists(
		IN const char* InterfaceName
	)
	{
		AurieModule* containing_module = nullptr;
		AurieInterfaceTableEntry* table_entry = nullptr;
		
		// If we find a module containing the interface, that means the interface exists!
		// ObpLookupInterfaceOwner will return AURIE_INTERFACE_NOT_FOUND if it doesn't exist.
		return AurieSuccess(
			Internal::ObpLookupInterfaceOwner(
				InterfaceName,
				true,
				containing_module,
				table_entry
			)
		);
	}

	namespace Internal
	{
		AurieStatus ObpDestroyInterfaceByName(
			IN const char* InterfaceName
		)
		{
			AurieModule* owner_module = nullptr;
			AurieInterfaceTableEntry* table_entry = nullptr;
			AurieStatus last_status = AURIE_SUCCESS;

			last_status = ObpLookupInterfaceOwner(
				InterfaceName,
				true,
				owner_module,
				table_entry
			);

			if (!AurieSuccess(last_status))
				return last_status;

			return ObpDestroyInterface(
				owner_module,
				table_entry->Interface,
				true
			);
		}

		AurieObjectType ObpGetObjectType(
			IN AurieObject* Object
		)
		{
			return Object->GetObjectType();
		}

		void ObpSetModuleOperationCallback(
			IN AurieModule* Module, 
			IN AurieModuleCallback CallbackRoutine
		)
		{
			Module->ModuleOperationCallback = CallbackRoutine;
		}

		void ObpDispatchModuleOperationCallbacks(
			IN AurieModule* AffectedModule, 
			IN AurieEntry Routine, 
			IN bool IsFutureCall
		)
		{
			// Determine the operation type
			// Yes I know, this is ugly, if you know a better solution
			// feel free to PR / tell me.
			AurieModuleOperationType current_operation_type = AURIE_OPERATION_UNKNOWN;

			if (Routine == AffectedModule->ModulePreinitialize)
				current_operation_type = AURIE_OPERATION_PREINITIALIZE;
			else if (Routine == AffectedModule->ModuleInitialize)
				current_operation_type = AURIE_OPERATION_INITIALIZE;
			else if (Routine == AffectedModule->ModuleUnload)
				current_operation_type = AURIE_OPERATION_UNLOAD;
			
			for (auto& loaded_module : g_LdrModuleList)
			{
				if (!loaded_module.ModuleOperationCallback)
					continue;

				loaded_module.ModuleOperationCallback(
					AffectedModule,
					current_operation_type,
					IsFutureCall
				);
			}
		}

		AurieStatus ObpAddInterfaceToTable(
			IN AurieModule* Module, 
			IN AurieInterfaceTableEntry& Entry
		)
		{
			Module->InterfaceTable.push_back(Entry);

			return AURIE_SUCCESS;
		}

		AurieStatus ObpDestroyInterface(
			IN AurieModule* Module, 
			IN AurieInterfaceBase* Interface,
			IN bool Notify)
		{
			if (Notify)
			{
				Interface->Destroy();
			}

			Module->InterfaceTable.remove_if(
				[Interface](const AurieInterfaceTableEntry& entry) -> bool
				{
					// Remove all interface entries with this one interface
					return entry.Interface == Interface;
				}
			);

			return AURIE_SUCCESS;
		}

		AurieStatus ObpLookupInterfaceOwner(
			IN const char* InterfaceName,
			IN bool CaseInsensitive,
			OUT AurieModule*& Module,
			OUT AurieInterfaceTableEntry*& TableEntry
		)
		{
			// Loop every single module
			for (auto& loaded_module : Internal::g_LdrModuleList)
			{
				// Check if we found it in this module
				auto iterator = std::find_if(
					loaded_module.InterfaceTable.begin(),
					loaded_module.InterfaceTable.end(),
					[CaseInsensitive, InterfaceName](const AurieInterfaceTableEntry& entry) -> bool
					{
						// Do a case insensitive comparison if needed
						if (CaseInsensitive)
						{
							return !_stricmp(entry.InterfaceName, InterfaceName);
						}

						return !strcmp(entry.InterfaceName, InterfaceName);
					}
				);

				// We found the interface in the current module!
				if (iterator != std::end(loaded_module.InterfaceTable))
				{
					Module = &loaded_module;
					TableEntry = &(*iterator);
					return AURIE_SUCCESS;
				}
			}

			// We didn't find any interface with that name.
			return AURIE_OBJECT_NOT_FOUND;
		}
	}

	AurieStatus ObGetInterface(
		IN const char* InterfaceName,
		OUT AurieInterfaceBase*& Interface
	)
	{
		AurieStatus last_status = AURIE_SUCCESS;
		AurieModule* owner_module = nullptr;
		AurieInterfaceTableEntry* interface_entry = nullptr;

		last_status = Internal::ObpLookupInterfaceOwner(
			InterfaceName,
			true,
			owner_module,
			interface_entry
		);

		if (!AurieSuccess(last_status))
			return last_status;

		Interface = interface_entry->Interface;
		return AURIE_SUCCESS;
	}

	AurieStatus ObDestroyInterface(
		IN AurieModule* Module,
		IN const char* InterfaceName
	)
	{
		AurieModule* owner_module = nullptr;
		AurieInterfaceTableEntry* table_entry = nullptr;

		AurieStatus last_status = Internal::ObpLookupInterfaceOwner(
			InterfaceName,
			true,
			owner_module,
			table_entry
		);

		if (!AurieSuccess(last_status))
			return last_status;

		if (owner_module != Module)
			return AURIE_ACCESS_DENIED;

		last_status = Internal::ObpDestroyInterface(
			Module,
			table_entry->Interface,
			true
		);

		return AURIE_SUCCESS;
	}
}

