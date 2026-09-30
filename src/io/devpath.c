#include "devpath.h"
#include <efilib.h>

EFI_DEVICE_PATH_PROTOCOL *LastMessagingNode(EFI_DEVICE_PATH_PROTOCOL *Path)
{
    EFI_DEVICE_PATH_PROTOCOL *Node = Path;
    EFI_DEVICE_PATH_PROTOCOL *Last = NULL;

    while (!IsDevicePathEnd(Node))
    {
        if (DevicePathType(Node) == MESSAGING_DEVICE_PATH)
            Last = Node;
        Node = NextDevicePathNode(Node);
    }
    return Last;
}