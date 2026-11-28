// 由运行时按包名调用：用国际服地址覆盖 profile。
#include "runtime_switch.hpp"
inline void apply_os_profile(){
  profile::singletonRoot = profile_os::singletonRoot;
  profile::graphicsFpsSlot = profile_os::graphicsFpsSlot;
  profile::vulkanResolverSlot = profile_os::vulkanResolverSlot;
  profile::engineOutputScreen = profile_os::engineOutputScreen;
  profile::tick = profile_os::tick;
  profile::setDevice = profile_os::setDevice;
  profile::setController = profile_os::setController;
  profile::selectMaps = profile_os::selectMaps;
  profile::mouseUpdate = profile_os::mouseUpdate;
  profile::keyboardUpdate = profile_os::keyboardUpdate;
  profile::escapeRoutePredicate = profile_os::escapeRoutePredicate;
  profile::escapeRouteCall = profile_os::escapeRouteCall;
  profile::escapeHandler = profile_os::escapeHandler;
  profile::uidTextSetter = profile_os::uidTextSetter;
  profile::textPopulateMesh = profile_os::textPopulateMesh;
  profile::objectName = profile_os::objectName;
  profile::stringFromUtf16 = profile_os::stringFromUtf16;
  profile::switchInputMode = profile_os::switchInputMode;
  profile::componentTransform = profile_os::componentTransform;
  profile::componentGameObject = profile_os::componentGameObject;
  profile::transformParent = profile_os::transformParent;
  profile::objectSetActive = profile_os::objectSetActive;
  profile::objectActiveSelf = profile_os::objectActiveSelf;
  profile::openPageByName = profile_os::openPageByName;
  profile::dailyClaimCallback = profile_os::dailyClaimCallback;
  profile::dailyRewardTaken = profile_os::dailyRewardTaken;
  profile::openExpedition = profile_os::openExpedition;
  profile::graphicsFrameRate = profile_os::graphicsFrameRate;
  profile::graphicsQualityFloat = profile_os::graphicsQualityFloat;
  profile::graphicsVulkanInitialize = profile_os::graphicsVulkanInitialize;
  profile::graphicsVulkanResolverLoad = profile_os::graphicsVulkanResolverLoad;
  profile::graphicsScreenWidth = profile_os::graphicsScreenWidth;
  profile::graphicsScreenHeight = profile_os::graphicsScreenHeight;
  profile::graphicsMainCamera = profile_os::graphicsMainCamera;
  profile::graphicsRebuildScene = profile_os::graphicsRebuildScene;
  profile::graphicsPostProcessUpdate = profile_os::graphicsPostProcessUpdate;
  profile::graphicsPrepareScene = profile_os::graphicsPrepareScene;
  profile::graphicsSetResolution = profile_os::graphicsSetResolution;
  profile::photoRead = profile_os::photoRead;
  profile::photoResize = profile_os::photoResize;
  profile::lookRefresh = profile_os::lookRefresh;
  profile::lookHorizontal = profile_os::lookHorizontal;
  profile::lookVertical = profile_os::lookVertical;
  profile::lookSpeed = profile_os::lookSpeed;
  profile::lookAxisSetting = profile_os::lookAxisSetting;
  profile::lookSourceLayout = profile_os::lookSourceLayout;
  profile::lookSpeedDefault = profile_os::lookSpeedDefault;
  profile::touchRotate = profile_os::touchRotate;
  profile::mapUpdateTouches = profile_os::mapUpdateTouches;
  profile::mapMouseHeld = profile_os::mapMouseHeld;
  profile::mapMouseDown = profile_os::mapMouseDown;
  profile::mapMouseUp = profile_os::mapMouseUp;
  profile::teamLoadingMoveNext = profile_os::teamLoadingMoveNext;
  profile::teamLoadingLayout = profile_os::teamLoadingLayout;
  profile::teamLoadingReady = profile_os::teamLoadingReady;
  profile::avatarFadeSetter = profile_os::avatarFadeSetter;
  profile::avatarFadeLayout = profile_os::avatarFadeLayout;
  profile::avatarFadeBinding = profile_os::avatarFadeBinding;
  profile::avatarFadeCamera = profile_os::avatarFadeCamera;
  profile::avatarFadeInline = profile_os::avatarFadeInline;
  profile::avatarFadeRecalculate = profile_os::avatarFadeRecalculate;
  profile::avatarFadeCombine = profile_os::avatarFadeCombine;
  profile::avatarSwitchCheck = profile_os::avatarSwitchCheck;
  profile::avatarSwitchFlag = profile_os::avatarSwitchFlag;
  profile::avatarSwitchCooldown = profile_os::avatarSwitchCooldown;
  profile::avatarSwitchMessage = profile_os::avatarSwitchMessage;
  profile::avatarSwitchRequest = profile_os::avatarSwitchRequest;
  profile::avatarSwitchRate = profile_os::avatarSwitchRate;
  profile::avatarSwitchReady = profile_os::avatarSwitchReady;
  profile::avatarSwitchReadyLayout = profile_os::avatarSwitchReadyLayout;
  profile::switchTraceUi = profile_os::switchTraceUi;
  profile::switchTraceData = profile_os::switchTraceData;
  profile::switchTraceCurrent = profile_os::switchTraceCurrent;
  profile::switchTraceTarget = profile_os::switchTraceTarget;
  profile::switchTraceSend = profile_os::switchTraceSend;
  profile::switchTraceToast = profile_os::switchTraceToast;
  profile::switchTraceWarning = profile_os::switchTraceWarning;
  profile::switchTraceInfo = profile_os::switchTraceInfo;
  profile::switchTraceInput = profile_os::switchTraceInput;
  profile::switchTraceResponse = profile_os::switchTraceResponse;
  profile::switchTraceCooldown = profile_os::switchTraceCooldown;
  profile::regionUnlockSetLocal = profile_os::regionUnlockSetLocal;
  profile::regionUnlockCacheSetter = profile_os::regionUnlockCacheSetter;
  profile::readyFlag = profile_os::readyFlag;
  profile::avatarFadePinsCount = profile_os::avatarFadePinsCount;
  profile::avatarSwitchPinsCount = profile_os::avatarSwitchPinsCount;
  profile::graphicsPinsCount = profile_os::graphicsPinsCount;
  profile::lookPinsCount = profile_os::lookPinsCount;
  profile::mapMousePinsCount = profile_os::mapMousePinsCount;
  profile::photoPinsCount = profile_os::photoPinsCount;
  profile::pinsCount = profile_os::pinsCount;
  profile::regionUnlockPinsCount = profile_os::regionUnlockPinsCount;
  profile::teamLoadingPinsCount = profile_os::teamLoadingPinsCount;
  profile::touchPinsCount = profile_os::touchPinsCount;
  { for(int i=0;i<5;++i) { profile::avatarFadePins[i].rva = profile_os::avatarFadePins[i].rva; __builtin_memcpy(profile::avatarFadePins[i].bytes, profile_os::avatarFadePins[i].bytes, 16); } }
  { for(int i=0;i<14;++i) { profile::avatarSwitchPins[i].rva = profile_os::avatarSwitchPins[i].rva; __builtin_memcpy(profile::avatarSwitchPins[i].bytes, profile_os::avatarSwitchPins[i].bytes, 16); } }
  { for(int i=0;i<10;++i) { profile::graphicsPins[i].rva = profile_os::graphicsPins[i].rva; __builtin_memcpy(profile::graphicsPins[i].bytes, profile_os::graphicsPins[i].bytes, 16); } }
  { for(int i=0;i<2;++i) { profile::lookPins[i].rva = profile_os::lookPins[i].rva; __builtin_memcpy(profile::lookPins[i].bytes, profile_os::lookPins[i].bytes, 16); } }
  { for(int i=0;i<4;++i) { profile::mapMousePins[i].rva = profile_os::mapMousePins[i].rva; __builtin_memcpy(profile::mapMousePins[i].bytes, profile_os::mapMousePins[i].bytes, 16); } }
  { for(int i=0;i<2;++i) { profile::photoPins[i].rva = profile_os::photoPins[i].rva; __builtin_memcpy(profile::photoPins[i].bytes, profile_os::photoPins[i].bytes, 16); } }
  { for(int i=0;i<19;++i) { profile::pins[i].rva = profile_os::pins[i].rva; __builtin_memcpy(profile::pins[i].bytes, profile_os::pins[i].bytes, 16); } }
  { for(int i=0;i<1;++i) { profile::regionUnlockPins[i].rva = profile_os::regionUnlockPins[i].rva; __builtin_memcpy(profile::regionUnlockPins[i].bytes, profile_os::regionUnlockPins[i].bytes, 16); } }
  { for(int i=0;i<3;++i) { profile::teamLoadingPins[i].rva = profile_os::teamLoadingPins[i].rva; __builtin_memcpy(profile::teamLoadingPins[i].bytes, profile_os::teamLoadingPins[i].bytes, 16); } }
}
