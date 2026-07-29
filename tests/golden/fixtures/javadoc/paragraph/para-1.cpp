// Testing paragraphs and
// removal of empty elements

// empty
void f1();

// also empty
/**
*/
void f2();

// should be empty
/**


*/
void f3();

/** brief
    
    a
      
    b 
     
    c 
*/
void f4();
