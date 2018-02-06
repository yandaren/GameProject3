﻿#include "stdafx.h"
#include "BagModule.h"
#include "DataPool.h"
#include "../ConfigData/ConfigStruct.h"
#include "GlobalDataMgr.h"
#include "../ConfigData/ConfigData.h"
#include "../Message/Game_Define.pb.h"
#include "../Message/Msg_ID.pb.h"
#include "EquipModule.h"
#include "PlayerObject.h"
#include "../ServerData/ServerDefine.h"
#include "PetModule.h"
#include "PartnerModule.h"
#include "RoleModule.h"
#include "GemModule.h"


CBagModule::CBagModule(CPlayerObject* pOwner): CModuleBase(pOwner)
{

}

CBagModule::~CBagModule()
{

}

BOOL CBagModule::OnCreate(UINT64 u64RoleID)
{

	return TRUE;
}


BOOL CBagModule::OnDestroy()
{
	for(auto itor = m_mapBagData.begin(); itor != m_mapBagData.end(); itor++)
	{
		itor->second->release();
	}

	m_mapBagData.clear();

	return TRUE;
}

BOOL CBagModule::OnLogin()
{
	return TRUE;
}

BOOL CBagModule::OnLogout()
{
	return TRUE;
}

BOOL CBagModule::OnNewDay()
{
	return TRUE;
}

BOOL CBagModule::ReadFromDBLoginData( DBRoleLoginAck& Ack )
{
	const DBBagData& BagData = Ack.bagdata();
	for(int i = 0; i < BagData.itemlist_size(); i++)
	{
		const DBBagItem& ItemData = BagData.itemlist(i);
		BagDataObject* pObject = g_pBagDataObjectPool->NewObject(FALSE);
		pObject->m_uGuid = ItemData.guid();
		pObject->m_uRoleID = ItemData.roleid();
		pObject->m_bBind = ItemData.bind();
		pObject->m_ItemGuid = ItemData.itemguid();
		pObject->m_ItemID = ItemData.itemid();
		m_mapBagData.insert(std::make_pair(pObject->m_uGuid, pObject));
	}

	return TRUE;
}

BOOL CBagModule::SaveToClientLoginData(RoleLoginAck& Ack)
{
	for(auto itor = m_mapBagData.begin(); itor != m_mapBagData.end(); itor++)
	{
		BagDataObject* pObject = itor->second;

		BagItem* pItem = Ack.add_bagitemlist();
		pItem->set_itemnum(pObject->m_nCount);
		pItem->set_bind(pObject->m_bBind);
		pItem->set_guid(pObject->m_uGuid);
		pItem->set_itemguid(pObject->m_ItemGuid);
		pItem->set_itemid(pObject->m_ItemID);
	}

	return TRUE;
}

BOOL CBagModule::CalcFightValue(INT32 nValue[PROPERTY_NUM], INT32 nPercent[PROPERTY_NUM], INT32& FightValue)
{
	return TRUE;
}

BOOL CBagModule::ReadFromShareMemory(BagDataObject* pObject)
{
	m_mapBagData.insert(std::make_pair(pObject->m_uGuid, pObject));
	return TRUE;
}

BOOL CBagModule::DispatchPacket(NetPacket* pNetPacket)
{
	return FALSE;
}

BOOL CBagModule::AddItem(UINT32 dwItemID, INT64 nCount)
{
	ERROR_RETURN_FALSE(dwItemID != 0);
	ERROR_RETURN_FALSE(nCount != 0);
	StItemInfo* pItemInfo = CConfigData::GetInstancePtr()->GetItemInfo(dwItemID);
	ERROR_RETURN_FALSE(pItemInfo != NULL);

	UINT64 uItemGuid = 0;
	INT64  nTempCount = nCount;
	switch(pItemInfo->dwItemType)
	{
		case EIT_EQUIP:
		{
			CEquipModule* pEquipModule = (CEquipModule*)m_pOwnPlayer->GetModuleByType(MT_EQUIP);
			ERROR_RETURN_FALSE(pEquipModule != NULL);
			uItemGuid = pEquipModule->AddEquip(dwItemID);
		}
		break;
		case EIT_GEM:
		{
			CGemModule* pGemModule = (CGemModule*)m_pOwnPlayer->GetModuleByType(MT_GEM);
			ERROR_RETURN_FALSE(pGemModule != NULL);
			uItemGuid = pGemModule->AddGem(dwItemID);
		}
		break;
		case EIT_PET:
		{
			CPetModule* pPetModule = (CPetModule*)m_pOwnPlayer->GetModuleByType(MT_PET);
			ERROR_RETURN_FALSE(pPetModule != NULL);
			uItemGuid = pPetModule->AddPet(dwItemID);

			//在这里要直接返回，因为宠物不进背包
			return TRUE;
		}
		break;
		case EIT_PARTNER:
		{
			CPartnerModule* pPartnerModule = (CPartnerModule*)m_pOwnPlayer->GetModuleByType(MT_PARTNER);
			ERROR_RETURN_FALSE(pPartnerModule != NULL);
			uItemGuid = pPartnerModule->AddPartner(dwItemID);

			//在这里要直接返回，因为伙伴不进背包
			return TRUE;
		}
		break;
		case EIT_ACTION:
		{
			CRoleModule* pRoleModule = (CRoleModule*)m_pOwnPlayer->GetModuleByType(MT_ROLE);
			ERROR_RETURN_FALSE(pRoleModule != NULL);
			pRoleModule->AddAction(dwItemID, nCount);
		}
		break;
		default:
		{
			for(auto itor = m_mapBagData.begin(); itor != m_mapBagData.end(); itor++)
			{
				BagDataObject* pTempObject = itor->second;

				if(pTempObject->m_ItemID != dwItemID)
				{
					continue;
				}
				INT64 nCanAdd = pItemInfo->StackMax - pTempObject->m_nCount;
				if( nCanAdd <= 0)
				{
					continue;
				}

				if (nTempCount <= nCanAdd)
				{
					pTempObject->lock();
					pTempObject->m_nCount += nTempCount;
					pTempObject->unlock();
					nTempCount = 0;
					m_setChange.insert(pTempObject->m_uGuid);
					break;
				}
				else
				{
					pTempObject->lock();
					pTempObject->m_nCount += nCanAdd;
					pTempObject->unlock();
					nTempCount -= nCanAdd;
					m_setChange.insert(pTempObject->m_uGuid);
				}
			}
		}
		break;
	}

	if (nTempCount <= 0)
	{
		return 0;
	}

	BagDataObject* pObject = g_pBagDataObjectPool->NewObject(TRUE);
	ERROR_RETURN_FALSE(pObject != NULL);
	pObject->lock();
	pObject->m_uGuid = CGlobalDataManager::GetInstancePtr()->MakeNewGuid();
	pObject->m_ItemGuid = uItemGuid;
	pObject->m_ItemID = dwItemID;
	pObject->m_nCount = nTempCount;
	pObject->m_uRoleID = m_pOwnPlayer->GetObjectID();
	pObject->unlock();
	m_mapBagData.insert(std::make_pair(pObject->m_uGuid, pObject));
	m_setChange.insert(pObject->m_uGuid);

	return TRUE;
}

BOOL CBagModule::AddItem(UINT64 uItemGuid, UINT32 dwItemID, INT64 nCount)
{
	ERROR_RETURN_FALSE(dwItemID != 0);
	ERROR_RETURN_FALSE(uItemGuid != 0);
	ERROR_RETURN_FALSE(nCount != 0);
	BagDataObject* pObject = g_pBagDataObjectPool->NewObject(TRUE);
	ERROR_RETURN_FALSE(pObject != NULL);
	pObject->lock();
	pObject->m_uGuid = CGlobalDataManager::GetInstancePtr()->MakeNewGuid();
	pObject->m_ItemGuid = uItemGuid;
	pObject->m_ItemID = dwItemID;
	pObject->m_nCount = nCount;
	pObject->m_uRoleID = m_pOwnPlayer->GetObjectID();
	pObject->unlock();
	m_mapBagData.insert(std::make_pair(pObject->m_uGuid, pObject));
	m_setChange.insert(pObject->m_uGuid);
	return TRUE;
}

BOOL CBagModule::RemoveItem(UINT32 dwItemID, INT64 nCount)
{
	ERROR_RETURN_FALSE(dwItemID != 0);
	INT64 nLeftCount = nCount;
	for(auto itor = m_mapBagData.begin(); itor != m_mapBagData.end(); )
	{
		if(nLeftCount <= 0)
		{
			return TRUE;
		}

		BagDataObject* pTempObject = itor->second;

		if(pTempObject->m_ItemID != dwItemID)
		{
			++itor;
			continue;
		}

		if(pTempObject->m_nCount <= nLeftCount)
		{
			pTempObject->m_nCount = 0;
			nLeftCount -= pTempObject->m_nCount;
			pTempObject->destroy();

			itor = m_mapBagData.erase(itor);
			continue;
		}
		else
		{
			pTempObject->m_nCount -= nLeftCount;
			nLeftCount = 0;
			return TRUE;
		}

		++itor;
	}

	return TRUE;
}

BOOL CBagModule::RemoveItem(UINT64 uGuid)
{
	ERROR_RETURN_FALSE(uGuid != 0);
	auto itor = m_mapBagData.find(uGuid);
	if (itor != m_mapBagData.end())
	{
		BagDataObject* pTempObject = itor->second;
		pTempObject->destroy();
		m_mapBagData.erase(uGuid);
		m_setRemove.insert(uGuid);
	}

	return TRUE;
}

BOOL CBagModule::SetBagItem(UINT64 uGuid, UINT64 uItemGuid, UINT32 dwItemID, INT64 nCount)
{
	ERROR_RETURN_FALSE(uGuid != 0);
	ERROR_RETURN_FALSE(uItemGuid != 0);
	auto itor = m_mapBagData.find(uGuid);
	if (itor != m_mapBagData.end())
	{
		BagDataObject* pTempObject = itor->second;
		pTempObject->lock();
		pTempObject->m_ItemGuid = uItemGuid;
		pTempObject->m_ItemID = dwItemID;
		pTempObject->m_nCount = nCount;
		pTempObject->unlock();
		m_setChange.insert(uGuid);
		return TRUE;
	}

	return FALSE;
}

INT64 CBagModule::GetItemCount(UINT32 dwItemID)
{
	INT64 nTotalCount = 0;

	for(auto itor = m_mapBagData.begin(); itor != m_mapBagData.end(); itor++)
	{
		BagDataObject* pTempObject = itor->second;

		if(pTempObject->m_ItemID != dwItemID)
		{
			continue;
		}

		nTotalCount += pTempObject->m_nCount;
	}

	return nTotalCount;
}

BagDataObject* CBagModule::GetItemByGuid(UINT64 uGuid)
{
	auto itor = m_mapBagData.find(uGuid);
	if(itor != m_mapBagData.end())
	{
		return itor->second;
	}

	return NULL;
}

BOOL CBagModule::NotifyChange()
{
	if (m_setChange.size() <= 0 && m_setRemove.size() <= 0)
	{
		return TRUE;
	}

	BagChangeNty Nty;
	for(auto itor = m_setChange.begin(); itor != m_setChange.end(); itor++)
	{
		BagDataObject* pObject = GetItemByGuid(*itor);
		ERROR_CONTINUE_EX(pObject != NULL);
		BagItem* pItem = Nty.add_changelist();
		pItem->set_guid(*itor);
		pItem->set_itemguid(pObject->m_ItemGuid);
		pItem->set_itemid(pObject->m_ItemID);
		pItem->set_itemnum(pObject->m_nCount);
	}

	for(auto itor = m_setRemove.begin(); itor != m_setRemove.end(); itor++)
	{
		Nty.add_removelist(*itor);
	}

	m_pOwnPlayer->SendMsgProtoBuf(MSG_BAG_CHANGE_NTY, Nty);

	m_setChange.clear();
	m_setRemove.clear();

	return TRUE;
}
