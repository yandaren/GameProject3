﻿#include "stdafx.h"
#include "MonsterCreator.h"
#include "Scene.h"

MonsterCreator::MonsterCreator(CScene* pScene)
{
	m_pScene = pScene;

	m_bAllFinished = FALSE;

	m_dwCurWave = -1;
}

MonsterCreator::~MonsterCreator()
{

}

BOOL MonsterCreator::ReadFromXml(rapidxml::xml_node<char>* pNode)
{
	for(auto pWaveNode = pNode->first_node("MapActWave"); pWaveNode != NULL; pWaveNode = pWaveNode->next_sibling("MapActWave"))
	{
		MonsterWave Wave;
		//Wave.m_dwGenType = CommonConvert::StringToInt(pWaveNode->first_attribute("gentype")->value());
		for(auto pObjectNode = pWaveNode->first_node("MapCallMonster"); pObjectNode != NULL; pObjectNode = pObjectNode->next_sibling("MapCallMonster"))
		{
			MonsterData Monster;
			Monster.m_dwActorID = CommonConvert::StringToInt(pObjectNode->first_attribute("MonsterID")->value());
			CommonConvert::StringToPos(pObjectNode->first_attribute("Pos")->value(), Monster.m_x, Monster.m_y, Monster.m_z);
			Monster.m_ft = CommonConvert::StringToFloat(pObjectNode->first_attribute("Face")->value());
			Monster.m_dwCamp = CommonConvert::StringToInt(pObjectNode->first_attribute("Camp")->value());
			//Monster.m_dwDropID = CommonConvert::StringToInt(pObjectNode->first_attribute("dropid")->value());
			Wave.m_vtMonsterList.push_back(Monster);
		}

		m_MonsterVaveList.push_back(Wave);
	}

	return TRUE;
}

BOOL MonsterCreator::OnUpdate(UINT64 uTick)
{
	if(m_dwCurWave == -1)
	{
		m_dwCurWave = 0;
		GenMonsterWave(0);
	}
	return TRUE;
}

BOOL MonsterCreator::GenMonsterWave(INT32 dwWaveIndex)
{
	if(dwWaveIndex >= (INT32)m_MonsterVaveList.size())
	{
		return FALSE;
	}

	MonsterWave& Wave = m_MonsterVaveList.at(dwWaveIndex);

	std::vector<MonsterData>  m_vtMonsterList;

	for( std::vector<MonsterData>::iterator itor = Wave.m_vtMonsterList.begin(); itor != Wave.m_vtMonsterList.end(); itor++)
	{
		MonsterData* pData = &(*itor);
		m_pScene->CreateMonster(pData->m_dwActorID, pData->m_dwCamp, pData->m_x, pData->m_y, pData->m_z, pData->m_ft);
	}

	return TRUE;
}

BOOL MonsterCreator::IsAllFinished()
{
	return m_bAllFinished;
}

BOOL MonsterCreator::OnObjectDie(CSceneObject* pObject)
{
	if(m_pScene->IsMonsterAllDie())
	{
		return FALSE;
	}

	GenMonsterWave(m_dwCurWave + 1);

	return TRUE;
}

