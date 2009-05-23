/*!
	@file
	@author		Albert Semenov
	@date		02/2008
	@module
*/
/*
	This file is part of MyGUI.
	
	MyGUI is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	MyGUI is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with MyGUI.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "MyGUI_Precompiled.h"
#include "MyGUI_Common.h"
#include "MyGUI_LayerManager.h"
#include "MyGUI_LayerItem.h"
#include "MyGUI_WidgetManager.h"
#include "MyGUI_RenderManager.h"
#include "MyGUI_Widget.h"

#include "MyGUI_SimpleLayerFactory.h"
#include "MyGUI_OverlappedLayerFactory.h"

namespace MyGUI
{

	const std::string XML_TYPE("Layer");

	MYGUI_INSTANCE_IMPLEMENT(LayerManager);

	void LayerManager::initialise()
	{
		MYGUI_ASSERT(false == mIsInitialise, INSTANCE_TYPE_NAME << " initialised twice");
		MYGUI_LOG(Info, "* Initialise: " << INSTANCE_TYPE_NAME);

		RenderManager::getInstance().setRenderQueueListener(this);
		WidgetManager::getInstance().registerUnlinker(this);
		ResourceManager::getInstance().registerLoadXmlDelegate(XML_TYPE) = newDelegate(this, &LayerManager::_load);

		addLayerFactory("SimpleLayer", new SimpleLayerFactory());
		addLayerFactory("OverlappedLayer", new OverlappedLayerFactory());

		MYGUI_LOG(Info, INSTANCE_TYPE_NAME << " successfully initialized");
		mIsInitialise = true;
	}

	void LayerManager::shutdown()
	{
		if (false == mIsInitialise) return;
		MYGUI_LOG(Info, "* Shutdown: " << INSTANCE_TYPE_NAME);

		removeLayerFactory("OverlappedLayer", true);
		removeLayerFactory("SimpleLayer", true);

		// удаляем все хранители слоев
		clear();

		WidgetManager::getInstance().unregisterUnlinker(this);
		ResourceManager::getInstance().unregisterLoadXmlDelegate(XML_TYPE);
		RenderManager::getInstance().setRenderQueueListener(nullptr);

		MYGUI_LOG(Info, INSTANCE_TYPE_NAME << " successfully shutdown");
		mIsInitialise = false;
	}

	void LayerManager::clear()
	{
		for (VectorLayer::iterator iter=mLayerKeepers.begin(); iter!=mLayerKeepers.end(); ++iter)
		{
			destroy(*iter);
		}
		mLayerKeepers.clear();
	}

	bool LayerManager::load(const std::string & _file, const std::string & _group)
	{
		return ResourceManager::getInstance()._loadImplement(_file, _group, true, XML_TYPE, INSTANCE_TYPE_NAME);
	}

	void LayerManager::_load(xml::ElementPtr _node, const std::string & _file, Version _version)
	{
		VectorLayer layers;
		// берем детей и крутимся, основной цикл
		xml::ElementEnumerator layer = _node->getElementEnumerator();
		while (layer.next(XML_TYPE))
		{

			std::string name;

			if ( false == layer->findAttribute("name", name))
			{
				MYGUI_LOG(Warning, "Attribute 'name' not found (file : " << _file << ")");
				continue;
			}

			for (VectorLayer::iterator iter=layers.begin(); iter!=layers.end(); ++iter)
			{
				MYGUI_ASSERT((*iter)->getName() != name, "Layer '" << name << "' already exist (file : " << _file << ")");
			}

			std::string type = layer->findAttribute("type"); 
			if (type.empty() && _version <= Version(1, 0))
			{
				bool overlapped = utility::parseBool(layer->findAttribute("overlapped"));
				type = overlapped ? "OverlappedLayer" : "SimpleLayer";
			}

			MapILayerFactory::iterator item = mLayerFactory.find(type);
			MYGUI_ASSERT(item != mLayerFactory.end(), "factory is '" << type << "' not found");

			layers.push_back(item->second->createLayer(layer.current(), _version));
		};

		// теперь мержим новые и старые слои
		merge(layers);
	}

	void LayerManager::_unlinkWidget(WidgetPtr _widget)
	{
		detachFromLayer(_widget);
	}

	// поправить на виджет и проверять на рутовость
	void LayerManager::attachToLayerKeeper(const std::string& _name, WidgetPtr _item)
	{
		MYGUI_ASSERT(nullptr != _item, "pointer must be valid");
		MYGUI_ASSERT(_item->isRootWidget(), "attached widget must be root");

		// сначала отсоединяем
		_item->detachFromLayer();

		// а теперь аттачим
		for (VectorLayer::iterator iter=mLayerKeepers.begin(); iter!=mLayerKeepers.end(); ++iter)
		{
			if (_name == (*iter)->getName())
			{
				ILayerNode* node = (*iter)->createItemNode(nullptr);
				node->attachLayerItem(_item);

				return;
			}
		}
		MYGUI_LOG(Error, "Layer '" << _name << "' is not found");
		//MYGUI_EXCEPT("Layer '" << _name << "' is not found");
	}

	void LayerManager::detachFromLayer(WidgetPtr _item)
	{
		MYGUI_ASSERT(nullptr != _item, "pointer must be valid");
		_item->detachFromLayer();
	}

	void LayerManager::upLayerItem(WidgetPtr _item)
	{
		MYGUI_ASSERT(nullptr != _item, "pointer must be valid");
		_item->upLayerItem();
	}

	bool LayerManager::isExist(const std::string & _name)
	{
		for (VectorLayer::iterator iter=mLayerKeepers.begin(); iter!=mLayerKeepers.end(); ++iter)
		{
			if (_name == (*iter)->getName()) return true;
		}
		return false;
	}

	void LayerManager::merge(VectorLayer & _layers)
	{
		for (VectorLayer::iterator iter=mLayerKeepers.begin(); iter!=mLayerKeepers.end(); ++iter)
		{
			if ((*iter) == nullptr) continue;
			bool find = false;
			std::string name = (*iter)->getName();
			for (VectorLayer::iterator iter2=_layers.begin(); iter2!=_layers.end(); ++iter2)
			{
				if (name == (*iter2)->getName())
				{
					// заменяем новый слой, на уже существующий
					delete (*iter2);
					(*iter2) = (*iter);
					(*iter) = nullptr;
					find = true;
					break;
				}
			}
			if (!find)
			{
				destroy(*iter);
				(*iter) = nullptr;
			}
		}

		// теперь в основной
		mLayerKeepers = _layers;
	}

	void LayerManager::destroy(ILayer* _layer)
	{
		MYGUI_LOG(Info, "destroy layer '" << _layer->getName() << "'");
		delete _layer;
	}

	bool LayerManager::isExistItem(ILayerNode * _item)
	{
		for (VectorLayer::iterator iter=mLayerKeepers.begin(); iter!=mLayerKeepers.end(); ++iter)
		{
			if ((*iter)->existItemNode(_item)) return true;
		}
		return false;
	}

	WidgetPtr LayerManager::getWidgetFromPoint(int _left, int _top)
	{
		VectorLayer::reverse_iterator iter = mLayerKeepers.rbegin();
		while (iter != mLayerKeepers.rend())
		{
			ILayerItem * item = (*iter)->getLayerItemByPoint(_left, _top);
			if (item != nullptr) return static_cast<WidgetPtr>(item);
			++iter;
		}
		return nullptr;
	}

	void LayerManager::addLayerFactory(const std::string& _name, ILayerFactory* _factory)
	{
		MapILayerFactory::const_iterator item = mLayerFactory.find(_name);
		MYGUI_ASSERT(item == mLayerFactory.end(), "factory is '" << _name << "' already exist");

		mLayerFactory[_name] = _factory;
	}

	void LayerManager::removeLayerFactory(ILayerFactory* _factory)
	{
		for (MapILayerFactory::iterator item=mLayerFactory.begin(); item!=mLayerFactory.end(); ++item)
		{
			if (item->second == _factory)
			{
				mLayerFactory.erase(item);
				return;
			}
		}
		MYGUI_EXCEPT("factory is '" << _factory << "' not found");
	}

	void LayerManager::removeLayerFactory(const std::string& _name, bool _delete)
	{
		MapILayerFactory::iterator item = mLayerFactory.find(_name);
		MYGUI_ASSERT(item != mLayerFactory.end(), "factory is '" << _name << "' not found");

		if (_delete) delete item->second;
		mLayerFactory.erase(item);
	}

	void LayerManager::doRender(bool _update)
	{
		for (VectorLayer::iterator iter=mLayerKeepers.begin(); iter!=mLayerKeepers.end(); ++iter)
		{
			(*iter)->doRender(_update);
		}
	}

} // namespace MyGUI
