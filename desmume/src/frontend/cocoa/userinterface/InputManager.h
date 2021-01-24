/*
	Copyright (C) 2013-2017 DeSmuME Team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>
#include <libkern/OSAtomic.h>
#include <IOKit/hid/IOHIDManager.h>
#include <ForceFeedback/ForceFeedback.h>

#include <unordered_map>
#include <string>
#include <vector>

#include "../audiosamplegenerator.h"
#include "../ClientInputHandler.h"

struct ClientCommandAttributes;
struct ClientInputDeviceProperties;
class MacInputDevicePropertiesEncoder;
@class EmuControllerDelegate;
@class InputManager;
@class InputHIDManager;

@protocol InputHIDManagerTarget <NSObject>

@required
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue hidManager:(InputHIDManager *)hidManager;

@end

typedef std::vector<ClientCommandAttributes> CommandAttributesList;

typedef std::unordered_map<std::string, ClientCommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = ClientCommandAttributes
typedef std::unordered_map<std::string, ClientCommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = ClientCommandAttributes
typedef std::unordered_map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
typedef std::unordered_map<int32_t, std::string> KeyboardKeyNameMap; // Key = Key code, Value = Key name

#pragma mark -
@interface InputHIDDevice : NSObject
{
	InputHIDManager *hidManager;
	IOHIDDeviceRef hidDeviceRef;
	IOHIDQueueRef hidQueueRef;
	
	NSString * identifier;
	
	io_service_t ioService;
	FFDeviceObjectReference ffDevice;
	FFEffectObjectReference ffEffect;
	BOOL supportsForceFeedback;
	BOOL isForceFeedbackEnabled;
	
	NSRunLoop *runLoop;
	OSSpinLock spinlockRunLoop;
}

@property (strong) InputHIDManager *hidManager;
@property (readonly) IOHIDDeviceRef hidDeviceRef;
@property (copy, readonly) NSString *manufacturerName;
@property (copy, readonly) NSString *productName;
@property (copy, readonly) NSString *serialNumber;
@property (copy, readonly) NSString *identifier;
@property (readonly) BOOL supportsForceFeedback;
@property (assign, getter=isForceFeedbackEnabled) BOOL forceFeedbackEnabled;
@property (strong) NSRunLoop *runLoop;

- (id) initWithDevice:(IOHIDDeviceRef)theDevice hidManager:(InputHIDManager *)theHIDManager;

- (void) setPropertiesUsingDictionary:(NSDictionary *)theProperties;
- (NSDictionary *) propertiesDictionary;
- (void) writeDefaults;

- (void) start;
- (void) stop;

- (void) startForceFeedbackAndIterate:(UInt32)iterations flags:(UInt32)ffFlags;
- (void) stopForceFeedback;

@end

bool InputElementCodeFromHIDElement(const IOHIDElementRef hidElementRef, char *charBuffer);
bool InputElementNameFromHIDElement(const IOHIDElementRef hidElementRef, char *charBuffer);
bool InputDeviceCodeFromHIDDevice(const IOHIDDeviceRef hidDeviceRef, char *charBuffer);
bool InputDeviceNameFromHIDDevice(const IOHIDDeviceRef hidDeviceRef, char *charBuffer);
ClientInputDeviceProperties InputAttributesOfHIDValue(IOHIDValueRef hidValueRef);
ClientInputDevicePropertiesList InputListFromHIDValue(IOHIDValueRef hidValueRef, InputManager *inputManager, bool forceDigitalInput);

size_t ClearHIDQueue(const IOHIDQueueRef hidQueue);
void HandleQueueValueAvailableCallback(void *inContext, IOReturn inResult, void *inSender);

#pragma mark -
@interface InputHIDManager : NSObject
{
	InputManager *inputManager;
	IOHIDManagerRef hidManagerRef;
	NSRunLoop *runLoop;
	NSArrayController *deviceListController;
	
	OSSpinLock spinlockRunLoop;
}

@property (strong) NSArrayController *deviceListController;
@property (strong) InputManager *inputManager;
@property (readonly) IOHIDManagerRef hidManagerRef;
@property (strong) id<InputHIDManagerTarget> target;
@property (nonatomic, strong) NSRunLoop *runLoop;

- (id) initWithInputManager:(InputManager *)theInputManager;

@end

void HandleDeviceMatchingCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);
void HandleDeviceRemovalCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);

#pragma mark -
@interface InputManager : NSObject
{
	EmuControllerDelegate *__weak emuControl;
	MacInputDevicePropertiesEncoder *inputEncoder;
	id<InputHIDManagerTarget> hidInputTarget;
	InputHIDManager *hidManager;
	NSMutableDictionary *inputMappings;
	NSArray *commandTagList;
	NSDictionary *commandIcon;
	
	InputCommandMap commandMap;
	CommandAttributesMap defaultCommandAttributes;
	AudioFileSampleGeneratorMap audioFileGenerators;
}

@property (weak) IBOutlet EmuControllerDelegate *emuControl;
@property (readonly) MacInputDevicePropertiesEncoder *inputEncoder;
@property (strong) id<InputHIDManagerTarget> hidInputTarget;
@property (readonly, strong) InputHIDManager *hidManager;
@property (readonly, strong) NSMutableDictionary *inputMappings;
@property (readonly, copy) NSArray *commandTagList;
@property (readonly, copy) NSDictionary *commandIcon;

- (void) setMappingsWithMappings:(NSDictionary *)mappings;
- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceDict commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingUsingInputAttributes:(const ClientInputDeviceProperties *)inputProperty commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingForIBAction:(const SEL)theSelector commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandAttributes:(const ClientCommandAttributes *)cmdAttr;

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) removeAllMappingsForCommandTag:(const char *)commandTag;

- (CommandAttributesList) generateCommandListUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList;
- (void) dispatchCommandList:(const CommandAttributesList *)cmdList;
- (BOOL) dispatchCommandUsingInputProperties:(const ClientInputDeviceProperties *)inputProperty;
- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector sender:(id)sender;

- (void) writeDefaultsInputMappings;
- (NSString *) commandTagFromInputList:(NSArray *)inputList;
- (ClientCommandAttributes) defaultCommandAttributesForCommandTag:(const char *)commandTag;

- (ClientCommandAttributes) mappedCommandAttributesOfDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) setMappedCommandAttributes:(const ClientCommandAttributes *)cmdAttr deviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) updateInputSettingsSummaryInDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo commandTag:(const char *)commandTag;

- (OSStatus) loadAudioFileUsingPath:(NSString *)filePath;
- (AudioSampleBlockGenerator *) audioFileGeneratorFromFilePath:(NSString *)filePath;
- (void) updateAudioFileGenerators;

@end

ClientCommandAttributes NewDefaultCommandAttributes(const char *commandTag);
ClientCommandAttributes NewCommandAttributesWithFunction(const char *commandTag, const ClientCommandDispatcher commandFunc);
ClientCommandAttributes NewCommandAttributesForDSControl(const char *commandTag, const NDSInputID controlID);
void UpdateCommandAttributesWithDeviceInfoDictionary(ClientCommandAttributes *cmdAttr, NSDictionary *deviceInfo);

NSMutableDictionary* DeviceInfoDictionaryWithCommandAttributes(const ClientCommandAttributes *cmdAttr,
															   NSString *deviceCode,
															   NSString *deviceName,
															   NSString *elementCode,
															   NSString *elementName);

void ClientCommandUpdateDSController(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSControllerWithTurbo(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSTouch(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSMicrophone(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSPaddle(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandAutoholdSet(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandAutoholdClear(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandLoadEmuSaveStateSlot(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandSaveEmuSaveStateSlot(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandCopyScreen(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandRotateDisplayRelative(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleAllDisplays(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandHoldToggleSpeedScalar(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleSpeedLimiter(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleAutoFrameSkip(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleCheats(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleExecutePause(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandCoreExecute(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandCorePause(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandFrameAdvance(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandFrameJump(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandReset(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleMute(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleGPUState(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);

class MacInputDevicePropertiesEncoder : public ClientInputDevicePropertiesEncoder
{
private:
	KeyboardKeyNameMap _keyboardNameTable;
	
public:
	MacInputDevicePropertiesEncoder();
	
	virtual ClientInputDeviceProperties EncodeKeyboardInput(const int32_t keyCode, bool keyPressed);
	virtual ClientInputDeviceProperties EncodeMouseInput(const int32_t buttonNumber, float touchLocX, float touchLocY, bool buttonPressed);
	virtual ClientInputDeviceProperties EncodeIBAction(const SEL theSelector, id sender);
	virtual ClientInputDevicePropertiesList EncodeHIDQueue(const IOHIDQueueRef hidQueue, InputManager *inputManager, bool forceDigitalInput);
};
