#include <fstream.h>
#include <conio.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include<process.h>

const int KeyFieldMax = 12;

const int KFMaxPlus1 = KeyFieldMax + 1;

const int DataFieldMax = 36;

const int DFMaxPlus1 = DataFieldMax + 1;

const int NULLCHAR = '\0';  // NULL character used to mark end of a string

typedef char KeyFieldType[KFMaxPlus1];

typedef char DataFieldType[DFMaxPlus1];

struct ItemType
   {
   KeyFieldType KeyField;
   DataFieldType DataField;
   };

const int MaxKeys = 11;   // max number of keys in a node

const int MaxKeysPlusOne = MaxKeys + 1;

const int MinKeys = 5;    // min number of keys in a node

const long NilPtr = -1L;   // the L indicates a long int

struct NodeType
   {
   int Count;               // Number of keys stored in the current node
   ItemType Key[MaxKeys];
   long Branch[MaxKeysPlusOne];   // pointers to child nodes
   };

void Error(char * msg);    //To output error

class BTTableClass
   {
   public:
      BTTableClass(char Mode, char * FileName);
      ~BTTableClass();
      int Empty() ;
      int Insert(ItemType & Item);
      int Retrieve(KeyFieldType SearchKey, ItemType & Item);
   private:
      int SearchNode(KeyFieldType Target, int & location);
      void AddItem(ItemType & NewItem, long NewRight,
	 NodeType & Node, int Location);
      void Split(ItemType & CurrentItem, long CurrentRight,
	 long CurrentRoot, int Location, ItemType & NewItem,
	 long & NewRight);
      void PushDown(ItemType & CurrentItem, long CurrentRoot,
	 int & MoveUp, ItemType & NewItem, long & NewRight);
      long Root;       // Pointer to the root node
      long NumNodes;   // number of nodes in the B-tree
      int NodeSize;    // number of bytes per node
      NodeType CurrentNode;   // storage for current node being worked on
   protected:
      fstream DataFile;   // the file stream for the table data
      long NumItems;      // number of records of type ItemType in the table
      char OpenMode;      // r or w (read or write) mode for the table
   };

void Error(char * msg)  //To print error msg and exit the program.
   {
   cout<<"\n"<<msg << "\n";
   exit(1);
   }
/*    constructor for a BTTableClass object.  If mode
      r is specified, it opens the table stored in the given file
      for reading.  If w is specified, it opens a new, empty table
      for writing (to the given file).  A new empty table contains
      a "dummy" node (node zero) that will be used to hold info
      about the whole table. */

BTTableClass::BTTableClass(char Mode, char * FileName)

   {

   OpenMode = Mode;
   NodeSize = sizeof(NodeType);

   if (Mode == 'r')
      {
      DataFile.open(FileName, ios::in | ios::binary);
      if (DataFile.fail())
	 Error("File cannot be opened");

      DataFile.read((char *)&CurrentNode , NodeSize);
      if (DataFile.fail())
	 {   // assuming the Btree is empty if we cannot read from the file
	 NumItems = 0;
	 NumNodes = 0;
	 Root = NilPtr;
	 }
      else   // Node zero is not a normal node, it contains the following:
	 {
	 NumItems = CurrentNode.Branch[0];
	 NumNodes = CurrentNode.Branch[1];
	 Root = CurrentNode.Branch[2];
	 }
      }
   else if (Mode == 'w')
      {
      DataFile.open(FileName, ios::in | ios::out | ios:: trunc |
	 ios::binary);
      if (DataFile.fail())
	 Error("File cannot be opened");

      Root = NilPtr;
      NumItems = 0;
      NumNodes = 0;   // number does not include the special node zero
      CurrentNode.Branch[0] = NumItems;
      CurrentNode.Branch[1] = NumNodes;
      CurrentNode.Branch[2] = Root;
      DataFile.seekp(0, ios::beg);
      DataFile.write((char *)&CurrentNode, NodeSize);
      }
   else
      Error("Incorrect mode given to BTTableClass constructor");
   }

/*
    This is the destructor for a BTTableClass object.  Its job
    is to destroy the BTTableClass object, while making sure that
    all of the table data is stored in the associated file.
*/

BTTableClass::~BTTableClass()
   {
      if (OpenMode == 'w')
      {
      // writing out the updated node zero:
      CurrentNode.Branch[0] = NumItems;
      CurrentNode.Branch[1] = NumNodes;
      CurrentNode.Branch[2] = Root;
      DataFile.seekp(0, ios::beg);
      DataFile.write((char *)&CurrentNode, NodeSize);
      }
      DataFile.close();
   }

/* Function to look for Target as a key in CurrentNode.
   Returns: true if found, false otherwise.
	    Location      The index of where Target was found.  If not
			  found, index and index + 1 are the indices between
			  which Target would fit.  (If Target fits to the
			  left of the first key, returns index of -1.)
*/
int BTTableClass::SearchNode(KeyFieldType Target, int & Location)
   {
   int Found;

   Found = 0;
   if (strcmp(Target, CurrentNode.Key[0].KeyField) < 0)
      Location = -1;
   else
      { // do a sequential search, right to left:
      Location = CurrentNode.Count - 1;
      while ((strcmp(Target, CurrentNode.Key[Location].KeyField) < 0)
	 && (Location > 0))
	 Location--;

      if (strcmp(Target, CurrentNode.Key[Location].KeyField) == 0)
	 Found = 1;
      }

   return Found;
   }


/* Function to add Item to Node at index Location, and add NewRight
   as the branch just to the right of NewItem.  The addition is
   made by moving the needed keys and branches right by 1 in order
   to clear out index Location for NewItem.
   Returns Updated node.
*/
void BTTableClass::AddItem(ItemType & NewItem, long NewRight, NodeType & Node,
			  int Location)
   {
   int j;

   for (j = Node.Count; j > Location; j--)
      {
      Node.Key[j] = Node.Key[j - 1];
      Node.Branch[j + 1] = Node.Branch[j];
      }

   Node.Key[Location] = NewItem;
   Node.Branch[Location + 1] = NewRight;
   Node.Count++;
   }


/* Given: The implicit BTTableClass object as well as:
	  CurrentItem    Item whose attempted placement into a node is
			 causing the node to be split.
	  CurrentRight   Pseudopointer to the child just to the right of
			 CurrentItem.
	  CurrentRoot    Pseudopointer to the node to be split.
	  Location       Index of where CurrentItem should go in this node.
  Task:   To split the node that CurrentRoot points to into 2 nodes,
	  pointed to by CurrentRoot and NewRight.  CurrentItem is properly
	  placed in 1 of these 2 nodes (unless it is the median that gets
	  moved up to the parent).  Finds Newitem, the median item that is
	  to be moved up to the parent node.
  Return: NewItem        The item to be moved up into the parent node.
	  NewRight       The pseudopointer to the child to the right of
			 NewItem (i.e. a pointer to the new RightNode).
*/
void BTTableClass::Split(ItemType & CurrentItem, long CurrentRight,
   long CurrentRoot, int Location, ItemType & NewItem, long & NewRight)
   {
   int j, Median;
   NodeType RightNode;
   if (Location < MinKeys)
      Median = MinKeys;
   else
      Median = MinKeys + 1;

   DataFile.seekg(CurrentRoot * NodeSize, ios::beg);
   DataFile.read((char *)&CurrentNode, NodeSize);
   for (j = Median; j < MaxKeys; j++)
      {  // move half of the items to the RightNode
      RightNode.Key[j - Median] = CurrentNode.Key[j];
      RightNode.Branch[j - Median + 1] = CurrentNode.Branch[j + 1];
      }

   RightNode.Count = MaxKeys - Median;
   CurrentNode.Count = Median;   // is then incremented by AddItem

   // put CurrentItem in place
   if (Location < MinKeys)
      AddItem(CurrentItem, CurrentRight, CurrentNode, Location + 1);
   else
      AddItem(CurrentItem, CurrentRight, RightNode,
	 Location - Median + 1);

   NewItem = CurrentNode.Key[CurrentNode.Count - 1];
   RightNode.Branch[0] = CurrentNode.Branch[CurrentNode.Count];
   CurrentNode.Count--;

   DataFile.seekp(CurrentRoot * NodeSize, ios::beg);
   DataFile.write((char *)&CurrentNode, NodeSize);
   NumNodes++;
   NewRight = NumNodes;
   DataFile.seekp(NewRight * NodeSize, ios::beg);
   DataFile.write((char *)&RightNode, NodeSize);
   }


/* Function to find where to put CurrentItem in a node of the subtree with
   the given root.  CurrentItem is ordinarily inserted, though
   a duplicate item is refused.  It is also possible that
   CurrentItem might be the item moved up to be inserted into
   the parent node if a split is done.
   Returns: MoveUp       True if NewItem (and associated NewRight pointer)
			 must be placed in the parent node due to
			 splitting, false otherwise.
	    NewItem      Item to be placed into parent node if a split was
			 done.
	    NewRight     pointer to child to the right of NewItem.
*/
void BTTableClass::PushDown(ItemType & CurrentItem, long CurrentRoot,
   int & MoveUp, ItemType & NewItem, long & NewRight)
   {
   int Location;
   if (CurrentRoot == NilPtr)   // stopping case
      {   // cannot insert into empty tree
      MoveUp = 1;
      NewItem = CurrentItem;
      NewRight = NilPtr;
      }
   else   // recursive case
      {
      DataFile.seekg(CurrentRoot * NodeSize, ios::beg);
      DataFile.read((char *)&CurrentNode, NodeSize);
      if (SearchNode(CurrentItem.KeyField, Location))
	 Error("Error: attempt to put a duplicate into B-tree");

      PushDown(CurrentItem, CurrentNode.Branch[Location + 1], MoveUp,
	 NewItem, NewRight);

      if (MoveUp)
	 {
	 DataFile.seekg(CurrentRoot * NodeSize, ios::beg);
	 DataFile.read((char *)&CurrentNode, NodeSize);
	 if (CurrentNode.Count < MaxKeys)
	    {
	    MoveUp = 0;
	    AddItem(NewItem, NewRight, CurrentNode, Location + 1);
	    DataFile.seekp(CurrentRoot * NodeSize, ios::beg);
	    DataFile.write((char *)&CurrentNode, NodeSize);
	    }
	 else
	    {
	    MoveUp = 1;
	    Split(NewItem, NewRight, CurrentRoot, Location,NewItem, NewRight);
	    }
	 }
      }
   }


// Function to add Item to the table.

int BTTableClass::Insert(ItemType & Item)
   {
   int MoveUp;
   long NewRight;
   ItemType NewItem;
   PushDown(Item, Root, MoveUp, NewItem, NewRight);

   if (MoveUp)   // create a new root node
      {
      CurrentNode.Count = 1;
      CurrentNode.Key[0] = NewItem;
      CurrentNode.Branch[0] = Root;
      CurrentNode.Branch[1] = NewRight;
      NumNodes++;
      Root = NumNodes;
      DataFile.seekp(NumNodes * NodeSize, ios::beg);
      DataFile.write((char *)&CurrentNode, NodeSize);
      }

   NumItems++;
   return 1;
   }


/* Function to look for SearchKey in the table.
   Returns: In the function name, true if SearchKey was found,
	    false otherwise.
	    Item        The item were SearchKey was found.
*/
int BTTableClass::Retrieve(KeyFieldType SearchKey, ItemType & Item)
   {
   long CurrentRoot;
   int Location;
   int Found;

   Found = 0;
   CurrentRoot = Root;

   while ((CurrentRoot != NilPtr) && (! Found))
      {
      DataFile.seekg(CurrentRoot * NodeSize, ios::beg);
      DataFile.read((char *)&CurrentNode, NodeSize);
      if (SearchNode(SearchKey, Location))
         {
	 Found = 1;
         Item = CurrentNode.Key[Location];
         }
      else
         CurrentRoot = CurrentNode.Branch[Location + 1];
      }

   return Found;
   }

//------------------------------------------------------------------------
//			Functions to make btree......

/* It reads dictionary data from the source text file btree.txt.  This file
   contains on each line a word (in upper case, up to 12 characters) and
   starting in column 13 the definition (up to 36 characters).  The
   B-tree-based table will be stored in the file btree.dat.
*/
const int LineMax = KeyFieldMax + DFMaxPlus1;

typedef char StringType[LineMax];


/* Function to read in a Word and its Definition from one line of this file.
   Returns: Word        In char array form, the word read in.
	    Definition  In char array form, the definition read in.
*/
void ReadLine(fstream & InputFile, KeyFieldType Word,
   DataFieldType Definition)
   {
   char Line[LineMax];
   int k, ch;

   InputFile.getline(Line, LineMax);   // will read the newline char

   for (k = 0; k < KeyFieldMax; k++)
      Word[k] = Line[k];
   Word[KeyFieldMax] = NULLCHAR;

   for (k = 0; k < DataFieldMax; k++)
      {
      ch = Line[KeyFieldMax + k];
      if (ch == '\n')
	 break;
      Definition[k] = ch;
      }
   Definition[k] = NULLCHAR;
  }


/* Function to read the data from InputFile and load it into the Table.
   Return: Table       The B-tree table containing the data.
*/
void Load(fstream & InputFile, BTTableClass & Table)
   {
   ItemType Item;
   ReadLine(InputFile, Item.KeyField, Item.DataField);
   while (! InputFile.fail())
      {
	Table.Insert(Item);
	ReadLine(InputFile, Item.KeyField, Item.DataField);
      }
   }
int btmake()
   {
   fstream fout;
   BTTableClass BTTable('w', "btree.dat");

   fout.open("binput.dat", ios::in);
   if (!fout)
      {
      cout<< "ERROR: Unable to open file binput.dat" << "\n";
      return 1;
      }

   Load(fout, BTTable);
   fout.close();
   return 0;
   }
//--------------------------------------------------------------------------



//---------------------------------------------------------------------------
//			    Functions to read btree........

/* Function to read in one word from the keyboard (or . to signal desire
	   to quit).
   Returns: Word   In char array form, the word read in, capitalized and
		  padded on the right with blanks so that it contains 12
		  characters.
	    In the function name, return true if a word was read in, false
	    if a . was read in.
*/

int ReadKeyboard(KeyFieldType Word)
   {
   int k, ch;
   int start;

   cin >> Word;
   cin.get();   // get the newline

   if (Word[0] == '.')
      return 0;

   start = 1;

   for (k = 0; k < KeyFieldMax; k++)
      {
      ch = Word[k];
      if (ch == '\0')
	 start = 0;
      if (start)
	 Word[k] = toupper(ch);   // capitalize
      else
	 Word[k] = ' ';   // pad with blanks
      }

   Word[KeyFieldMax] = NULLCHAR;
   return 1;
   }


int btread()
   {
   ItemType Item;
   KeyFieldType SearchKey;
   BTTableClass BTTable('r', "btree.dat");
   cout << "Enter the word to be looked up (or . to quit): ";

   while (ReadKeyboard(SearchKey))
      {
      if (BTTable.Retrieve(SearchKey, Item))
	 cout << " Definition:   " << Item.DataField << endl;
      else
	 cout << " Not found" << endl;
      cout << endl << "Enter the word to be looked up (or . to quit): ";
      }

   return 0;
   }
//--------------------------------------------------------------------------

//============= MAIN FUNCTION========================
void main()
   {
   clrscr();
   int cm,cr;
   cm=btmake();
   if(cm==0)
	cr=btread();
   getch();
   }
