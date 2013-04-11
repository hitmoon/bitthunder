/**
 *	BitThunder - File-system Manager.
 *
 **/
#include <bitthunder.h>
#include <string.h>

BT_DEF_MODULE_NAME			("Filesystem Manager")
BT_DEF_MODULE_DESCRIPTION	("Filesystem Mountpoint management")
BT_DEF_MODULE_AUTHOR	  	("James Walmsley")
BT_DEF_MODULE_EMAIL			("james@fullfat-fs.co.uk")

static BT_LIST g_oFileSystems = {0};
static BT_LIST g_oMountPoints = {0};

struct _BT_OPAQUE_HANDLE {
	BT_HANDLE_HEADER h;
};

typedef struct _BT_FILESYSTEM {
	BT_LIST_ITEM oItem;
	BT_HANDLE	 hFS;
} BT_FILESYSTEM;

typedef struct _BT_MOUNTPOINT {
	BT_LIST_ITEM 	oItem;
	BT_HANDLE 		hMount;
	BT_i8 		   *szpPath;
	BT_HANDLE 		hFS;
} BT_MOUNTPOINT;

BT_ERROR BT_RegisterFilesystem(BT_HANDLE hFS) {
	if(hFS->h.pIf->eType != BT_HANDLE_T_FILESYSTEM) {
		return BT_ERR_GENERIC;
	}

	BT_FILESYSTEM *pFilesystem = BT_kMalloc(sizeof(BT_FILESYSTEM));
	if(!pFilesystem) {
		return BT_ERR_NO_MEMORY;
	}

	pFilesystem->hFS = hFS;

	BT_ListAddItem(&g_oFileSystems, &pFilesystem->oItem);

	return BT_ERR_NONE;
}

static BT_MOUNTPOINT *GetMountPoint(const BT_i8 *szpPath) {
	BT_MOUNTPOINT *pMountPoint = (BT_MOUNTPOINT *) BT_ListGetHead(&g_oMountPoints);
	while(pMountPoint) {
		BT_i8 *common = (BT_i8 *) strstr(szpPath, pMountPoint->szpPath);
		if(common == szpPath) {	// Ensure the common section is from root of szpPath!
			return pMountPoint;
		}

		pMountPoint = (BT_MOUNTPOINT *) BT_ListGetNext(&pMountPoint->oItem);
	}

	return pMountPoint;
}

BT_ERROR BT_Mount(BT_HANDLE hVolume, const BT_i8 *szpPath) {
	BT_ERROR Error;

	if(hVolume->h.pIf->eType != BT_HANDLE_T_VOLUME &&
	   hVolume->h.pIf->eType != BT_HANDLE_T_PARTITION &&
	   hVolume->h.pIf->eType != BT_HANDLE_T_BLOCK) {

		return BT_ERR_GENERIC;
	}

	BT_HANDLE hMount = NULL;

	// Is path already mounted?
	if(GetMountPoint(szpPath)) {
		return BT_ERR_GENERIC;
	}

	// Can any file-system mount this partition?
	BT_FILESYSTEM *pFilesystem = (BT_FILESYSTEM *) BT_ListGetHead(&g_oFileSystems);
	while(pFilesystem) {
		const BT_IF_FS *pFs = pFilesystem->hFS->h.pIf->oIfs.pFilesystemIF;
		hMount = pFs->pfnMount(pFilesystem->hFS, hVolume, &Error);
		if(hMount) {
			break;
		}

		pFilesystem = (BT_FILESYSTEM *) BT_ListGetNext(&pFilesystem->oItem);
	}

	if(!hMount) {
		BT_kPrint("FS: Could not mount volume, no compatible filesystem.");
		return BT_ERR_GENERIC;
	}

	// A filesystem was able to mount the volume, now we can handle this under our own namespace.
	BT_MOUNTPOINT *pMountPoint = (BT_MOUNTPOINT *) BT_kMalloc(sizeof(BT_MOUNTPOINT));
	if(!pMountPoint) {
		Error = BT_ERR_GENERIC;
		goto err_unmount_out;
	}

	pMountPoint->hMount 	= hMount;
	pMountPoint->hFS 		= pFilesystem->hFS;
	pMountPoint->szpPath 	= BT_kMalloc(strlen(szpPath) + 1);
	strcpy(pMountPoint->szpPath, szpPath);

	return BT_ERR_NONE;

err_unmount_out:
	pFilesystem->hFS->h.pIf->oIfs.pFilesystemIF->pfnUnmount(hMount);

	return Error;
}


static BT_ERROR bt_fs_init() {
	BT_ListInit(&g_oFileSystems);
	BT_ListInit(&g_oMountPoints);
	return BT_ERR_NONE;
}

BT_MODULE_INIT_DEF oModuleEntry = {
	BT_MODULE_NAME,
	bt_fs_init,
};
