#include "stdafx.h"

#include "..\_Rbcs\RBCSDIMENSIONING\BaseData\Element.cpp"
#include "..\_Rbcs\RBCSDIMENSIONING\BaseData\Link.cpp"
#include "..\_Rbcs\RBCSDIMENSIONING\BaseData\LinkManager.cpp"
#include "..\_Rbcs\RBCSDIMENSIONING\BaseData\Tree.cpp"


using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;
using namespace	Microsoft::VisualStudio::TestTools::UnitTesting;
using namespace dim::data;

namespace tests
{
	[TestClass]
	public ref class RbCSDimensioning_BaseData
	{
	public:
		#pragma region Additional test attributes
		//
		//You can use the following additional attributes as you write your tests:
		//
		//Use ClassInitialize to run code before running the first test in the class
		//[ClassInitialize()]
		//static void MyClassInitialize(TestContext^ testContext) {};
		//
		//Use ClassCleanup to run code after all tests in a class have run
		//[ClassCleanup()]
		//static void MyClassCleanup() {};
		//
		//Use TestInitialize to run code before running each test
		//[TestInitialize()]
		//void MyTestInitialize() {};
		//
		//Use TestCleanup to run code after each test has run
		//[TestCleanup()]
		//void MyTestCleanup() {};
		//
//---------------------------------------------------------------------------------------------------------------
		[TestMethod]
		void Test1Tree()
		{
			CTree obTree;
			Assert::AreEqual( obTree.GetStatus() == Ok, true );//создалось ли дерево
			Assert::AreEqual(obTree.GetSize(), 0);//кол-во эл-ов в дереве должно быть равным нулю 

			Handle hRoot = obTree.Insert( CElement(11), 0 )->GetHandle();//добавляем первый элемент в дерево к несуществующему элементу с 0 хэндлом 
			Assert::AreEqual( obTree.GetSize(), 1 );//кол-во эл-ов в дереве должно стать равным 1

			Handle h2 = obTree.Insert( CElement(22), hRoot )->GetHandle();//добавляем ещё один элемент в дерево и подсоединяем его к корню 
			Assert::AreEqual( obTree.GetSize(), 2 );//кол-во эл-ов в дереве должно стать равным 2
			
			CElement::Ptr p2bad = obTree.Insert( CElement(22), hRoot );//пытаемся добавить тот же элемент второй раз, и подсоединить его опять к корню
			Assert::IsTrue( p2bad ==0 );// элемент не добавился
			Assert::AreEqual( obTree.GetSize(), 2 );//количество эл-ов не должно измениться
			
			CElement::Ptr p_0 = obTree.Insert( CElement(222), -1 );//пытаемся подсоединить новый элемент к элементу с несуществующим хэндлом
			Assert::IsTrue( p_0 == 0 );// элемент не добавился
			Assert::AreEqual( obTree.GetSize(), 2 );//количество эл-ов не должно измениться
			
			Handle h3 = obTree.Insert( CElement(33), h2 )->GetHandle();//пытаемся подсоединить новый элемент ко второму элементу
			Assert::IsTrue( h3 == 33 );// должен возвратиться тот же хэндл
			Assert::AreEqual( obTree.GetSize(), 3 );////количество эл-ов должно стать равным 3
			
			h3 = obTree.Insert( CElement(33), hRoot )->GetHandle();//пытаемся подсоединить новый элемент к корню
			Assert::IsTrue( h3 == 33 );// должен возвратиться тот же хэндл
			Assert::AreEqual( obTree.GetSize(), 3 );//количество эл-ов не должно увеличиться
			
			CElement::Ptr p3bad = obTree.Insert( CElement(33), h3 );//пытаемся подсоединить элемент к самому себе
			Assert::IsTrue( p3bad == 0 );// элемент не добавился
			Assert::AreEqual( obTree.GetSize(), 3 );//количество эл-ов не должно увеличиться
			
			CElement::Ptr pRoot_bad = obTree.Insert( CElement(666), 0 );//пытаемся добавить ещё один корневой элемент
			Assert::IsTrue( pRoot_bad == 0 );// должен возвратиться отрицательный хэндл
			Assert::AreEqual( obTree.GetSize(), 3 );//количество эл-ов не должно увеличиться
			
		}
//---------------------------------------------------------------------------------------------------------------
		[TestMethod]
		void Test2Links()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(11), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Assert::IsTrue(obTree.HasLink(hRoot,h2));//проверяем существует ли связь между двумя связанными элементами
			Assert::IsTrue(obTree.HasLink(h2,hRoot));//проверяем имеет ли значение направление связи
			
			Handle h3 = obTree.Insert(CElement(33), h2)->GetHandle();
			Assert::IsTrue(obTree.HasLink(h3, h2));//проверяем существует ли всязь между другими двумя связанными  элементами
			Assert::IsFalse(obTree.HasLink(h3, hRoot));//проверяем существует ли связь между не связанными элементами
			Assert::IsFalse(obTree.HasLink(hRoot, h3));//проверяем имеет ли при этом значение направление связи
			
			obTree.Insert(CElement(33), h3); //пытаемся подсоединить элемент к самому себе
			Assert::IsFalse(obTree.HasLink(h3, h3));//не должно существовать такой связи
		}
//---------------------------------------------------------------------------------------------------------------
		[TestMethod]
		void Test3IsParent()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(11), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Assert::IsTrue(obTree.IsParent(hRoot,h2));
			Assert::IsFalse(obTree.IsParent(h2,hRoot));
		}
//---------------------------------------------------------------------------------------------------------------
		[TestMethod]
		void Test4RemoveLink()
		{
			CTree obTree;
			Handle hRoot	 = obTree.Insert(CElement(11), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Handle h3 = obTree.Insert(CElement(33), h2)->GetHandle();
			
			Assert::IsTrue(obTree.RemoveLink(h3,h2));
			Assert::IsFalse(obTree.HasLink(h3,h2));

			Assert::IsTrue(obTree.RemoveLink(hRoot,h2));			
			Assert::IsFalse(obTree.HasLink(hRoot,h2));
			
		}
		
		[TestMethod]
		void Test5RemoveElement()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(11), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Handle h3 = obTree.Insert(CElement(33), h2)->GetHandle();
			h3 = obTree.Insert(CElement(33), hRoot)->GetHandle();
			Assert::IsTrue(obTree.Remove(h3));
			Assert::AreEqual( obTree.GetSize(), 2 );
			Assert::IsTrue(obTree.HasLink(hRoot,h2));
			Assert::IsFalse(obTree.HasLink(h3,h2));
			Assert::IsFalse(obTree.HasLink(hRoot,h3));
			
			Handle h4 = obTree.Insert(CElement(44), h2)->GetHandle();
			Assert::IsTrue(obTree.Remove(h2));
			Assert::AreEqual( obTree.GetSize(), 2 );
			Assert::IsFalse(obTree.HasLink(hRoot,h2));
			Assert::IsFalse(obTree.HasLink(h4,h2));
			Assert::IsFalse(obTree.HasLink(hRoot,h4));
			
		}
//---------------------------------------------------------------------------------------------------------------
		[TestMethod]
		void Test6GetAllChildrenAndParent()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(11), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Handle h3 = obTree.Insert(CElement(33), hRoot)->GetHandle();
			Handle h4 = obTree.Insert(CElement(44), hRoot)->GetHandle();
			h4 = obTree.Insert(CElement(44), h2)->GetHandle();
			h4 = obTree.Insert(CElement(44), h3)->GetHandle();
			
			std::vector<Handle> EtalonOfChildrens, EtalonOfParents, Childs, Parents;

			EtalonOfChildrens.push_back(h2);
			EtalonOfChildrens.push_back(h3);
			EtalonOfChildrens.push_back(h4);
			std::sort(EtalonOfChildrens.begin(), EtalonOfChildrens.end());
			Assert::AreEqual(obTree.GetAllChildren (Childs, hRoot), 3);
			std::sort(Childs.begin(), Childs.end());
			Assert::AreEqual(std::equal(EtalonOfChildrens.begin(), EtalonOfChildrens.end(), Childs.begin()), true);

			EtalonOfParents.push_back(hRoot);
			EtalonOfParents.push_back(h2);
			EtalonOfParents.push_back(h3);
			std::sort(EtalonOfParents.begin(), EtalonOfParents.end());
			Assert::AreEqual(obTree.GetAllParents (Parents, h4), 3);
			std::sort(Parents.begin(), Parents.end());
			Assert::AreEqual(std::equal(EtalonOfParents.begin(), EtalonOfParents.end(), Parents.begin()), true);

		}

		[TestMethod]
		void Test7LinksCount()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(11), 0)->GetHandle();
			Assert::AreEqual(obTree.LinksCount(hRoot),0);
			
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Handle h3 = obTree.Insert(CElement(33), hRoot)->GetHandle();
			h3 = obTree.Insert(CElement(33), h2)->GetHandle();
			Handle h4 = obTree.Insert(CElement(44), hRoot)->GetHandle();
			h4 = obTree.Insert(CElement(44), h2)->GetHandle();
			h4 = obTree.Insert(CElement(44), h3)->GetHandle();
			Assert::AreEqual(obTree.LinksCount(h3),3);
			
			Handle h5 = obTree.Insert(CElement(55), h4)->GetHandle();
			Assert::AreEqual(obTree.LinksCount(h5),1);
		}
		
		[TestMethod]
		void Test8RemoveSubTree()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(11), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(22), hRoot)->GetHandle();
			Handle h3 = obTree.Insert(CElement(33), hRoot)->GetHandle();
			Handle h4 = obTree.Insert(CElement(44), h3)->GetHandle();
			Handle h5 = obTree.Insert(CElement(55), h3)->GetHandle();
			h4 = obTree.Insert(CElement(44), h2)->GetHandle();
			obTree.RemoveSubTree(hRoot);			
			Assert::AreEqual(obTree.LinksCount(hRoot),0);
			Assert::AreEqual( obTree.GetSize(), 0 );
		}	
		
		[TestMethod]
		void Test9GetUnlinkedList()
		{
			CTree obTree;
			Handle hRoot = obTree.Insert(CElement(1), 0)->GetHandle();
			Handle h2 = obTree.Insert(CElement(2), hRoot)->GetHandle();
			Handle h21 = obTree.Insert(CElement(21), h2)->GetHandle();
			Handle h22 = obTree.Insert(CElement(22), h2)->GetHandle();
			Handle h23 = obTree.Insert(CElement(23), h2)->GetHandle();
			Handle h3 = obTree.Insert(CElement(3), hRoot)->GetHandle();
			Handle h31 = obTree.Insert(CElement(31), h3)->GetHandle();
			Handle h32 = obTree.Insert(CElement(32), h3)->GetHandle();
			Assert::IsTrue(obTree.Remove(h2));
						
			std::vector<Handle> EtalonOfUnlinkedList, UnlinkedList;			
			
			EtalonOfUnlinkedList.push_back(h21);
			EtalonOfUnlinkedList.push_back(h22);
			EtalonOfUnlinkedList.push_back(h23);
			
			std::sort(EtalonOfUnlinkedList.begin(), EtalonOfUnlinkedList.end());
			Assert::AreEqual(obTree.GetUnlinkedList(UnlinkedList), 3);
			std::sort(UnlinkedList.begin(), UnlinkedList.end());
			Assert::IsTrue(std::equal(EtalonOfUnlinkedList.begin(), EtalonOfUnlinkedList.end(), UnlinkedList.begin()));
			
			Assert::IsTrue(obTree.Remove(h3));
			EtalonOfUnlinkedList.push_back(h31);
			EtalonOfUnlinkedList.push_back(h32);
			
			std::sort(EtalonOfUnlinkedList.begin(), EtalonOfUnlinkedList.end());
			Assert::AreEqual(obTree.GetUnlinkedList(UnlinkedList), 5);
			std::sort(UnlinkedList.begin(), UnlinkedList.end());
			Assert::IsTrue(std::equal(EtalonOfUnlinkedList.begin(), EtalonOfUnlinkedList.end(), UnlinkedList.begin()));
			
			
		}  
	};
}
//---------------------------------------------------------------------------------------------------------------