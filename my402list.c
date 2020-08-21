#include "my402list.h"

#include <stdlib.h>

int My402ListLength(My402List* listobject)
{
	return listobject -> num_members;
}

int  My402ListEmpty(My402List* listobject)
{
	return listobject -> num_members <= 0;
}

int  My402ListAppend(My402List* listobject, void* dataobject)
{
	My402ListElem *elementobject = (My402ListElem *) malloc(sizeof(My402ListElem));
	if(elementobject)
	{
		listobject -> num_members++;
		elementobject -> obj = dataobject;
		if(My402ListEmpty(listobject))
		{
			(listobject -> anchor).next = elementobject;
			elementobject -> next = &(listobject -> anchor);
			(listobject -> anchor).prev = elementobject;
			elementobject -> prev = &(listobject -> anchor);
		}
		else
		{
			My402ListElem *lastpointer = (listobject -> anchor).prev;
			lastpointer -> next = elementobject;
			(listobject -> anchor).prev = elementobject;
			elementobject -> prev = lastpointer;
			elementobject -> next = &(listobject -> anchor);
		}
		return 1;
	}
	else
	{
		return 0;	
	}
}

int  My402ListPrepend(My402List* listobject, void* dataobject)
{
	My402ListElem *elementobject = (My402ListElem *) malloc(sizeof(My402ListElem));
	if(elementobject)
	{
		listobject -> num_members++;
		elementobject -> obj = dataobject;
		if(My402ListEmpty(listobject))
		{
			(listobject -> anchor).next = elementobject;
			elementobject -> prev = &(listobject -> anchor);
			(listobject -> anchor).prev = elementobject;
			elementobject -> next = &(listobject -> anchor);
		}
		else
		{
			My402ListElem *firstpointer = (listobject -> anchor).next;
			(listobject -> anchor).next = elementobject;
			firstpointer -> prev = elementobject;
			elementobject -> prev = &(listobject -> anchor);
			elementobject -> next = firstpointer;
		}
		return 1;
	}
	else
	{
		return 0;		
	}
}

void My402ListUnlink(My402List* listobject, My402ListElem* elementobject)
{
	if(My402ListEmpty(listobject))
	{
		return;
	}
	listobject -> num_members--;
	My402ListElem *previouspointer = elementobject -> prev, *nextpointer = elementobject -> next;
	free(elementobject);
	previouspointer -> next = nextpointer;
	nextpointer -> prev = previouspointer;
}

void My402ListUnlinkAll(My402List* listobject)
{
	if(My402ListEmpty(listobject))
	{
		return;
	}
	My402ListElem *currentpointer = (listobject -> anchor).next;
	for(currentpointer = (listobject -> anchor).next; currentpointer != &(listobject -> anchor); currentpointer = currentpointer -> next)
	{
		My402ListElem *temppointer = currentpointer;
		My402ListUnlink(listobject, temppointer);
	}
}

int  My402ListInsertAfter(My402List* listobject, void* dataobject, My402ListElem* elementpointer)
{
	if(elementpointer)
	{
		My402ListElem *newelementobject = (My402ListElem *) malloc(sizeof(My402ListElem));
		if(newelementobject)
		{
			listobject -> num_members++;
			newelementobject -> obj = dataobject;
			My402ListElem *nextelementpointer = elementpointer -> next;
			newelementobject -> next = nextelementpointer;
			elementpointer -> next = newelementobject;
			nextelementpointer -> prev = newelementobject;
			newelementobject -> prev = elementpointer;
			return 1;
		}
		else
			return 0;
	}
	else
	{
		return My402ListAppend(listobject, dataobject);
	}
}

int  My402ListInsertBefore(My402List* listobject, void* dataobject, My402ListElem* elementpointer)
{
	if(elementpointer)
	{
		My402ListElem *newelementobject = (My402ListElem *) malloc(sizeof(My402ListElem));
		if(newelementobject)
		{
			listobject -> num_members++;
			newelementobject -> obj = dataobject;
			My402ListElem *previouselementpointer = elementpointer -> prev;
			newelementobject -> prev = previouselementpointer;
			elementpointer -> prev = newelementobject;
			previouselementpointer -> next = newelementobject;
			newelementobject -> next = elementpointer;
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return My402ListPrepend(listobject, dataobject);
	}
}

My402ListElem *My402ListFirst(My402List* listobject)
{
	if(My402ListEmpty(listobject))
	{
		return NULL;
	}
	else
	{
		return (listobject -> anchor).next;
	}
}

My402ListElem *My402ListLast(My402List* listobject)
{
	if(My402ListEmpty(listobject))
	{
		return NULL;
	}
	else
	{
		return (listobject -> anchor).prev;
	}
}

My402ListElem *My402ListNext(My402List* listobject, My402ListElem* elementpointer)
{
	if(elementpointer)
	{
		if(elementpointer == My402ListLast(listobject))
		{
			return NULL;
		}
		else
		{
			return elementpointer -> next;
		}
	}
	else
	{
		return My402ListFirst(listobject);
	}
}

My402ListElem *My402ListPrev(My402List* listobject, My402ListElem* elementpointer)
{
	if(elementpointer)
	{
		if(elementpointer == My402ListFirst(listobject))
		{
			return NULL;
		}
		else
		{
			return elementpointer -> prev;
		}
	}
	else
	{
		return My402ListLast(listobject);
	}
}

My402ListElem *My402ListFind(My402List* listobject, void* dataobject)
{
	My402ListElem *currentpointer = NULL;
	for(currentpointer = My402ListFirst(listobject); currentpointer != NULL; currentpointer = My402ListNext(listobject, currentpointer))
	{
		if(currentpointer -> obj == dataobject)
		{
			return currentpointer;
		}
	}
	return NULL;
}

int My402ListInit(My402List* listobject)
{
	listobject -> num_members = 0;
	(listobject -> anchor).next = &(listobject -> anchor);
	(listobject -> anchor).prev = &(listobject -> anchor);
	(listobject -> anchor).obj = NULL;
	return 1;
}
