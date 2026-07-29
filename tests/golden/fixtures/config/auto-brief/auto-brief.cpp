// No documentation
void noDocFunction();

// Documentation with no brief
/**
 * This function has documentation but no brief.
 */
void docNoBriefFunction();

// Explicit brief
/**
 * @brief This is the explicit brief.
 *
 * This description will never be copied as
 * brief because it is an explicit brief.
 */
void explicitBriefFunction();

/**
 * This will not be copied as brief.
 *
 * @brief This is the explicit brief.
 */
void explicitBriefFunction2();

// First sentence as brief and more documentation
/**
 * This is the brief.
 *
 * This is more documentation.
 */
void firstSentenceAsBriefFunction();

// Copy brief
/**
 * \copybrief noDocFunction
 */
void failCopyBriefFromNoDoc();

/**
 * \copybrief docNoBriefFunction
 */
void failCopyBriefFromDocNoBrief();

/**
 * \copybrief explicitBriefFunction
 */
void copyBriefFromExplicitBrief();

/**
 * \copybrief noDocFunction
 * \copybrief docNoBriefFunction
 * \copybrief explicitBriefFunction
 */
void copyBriefFromFirstValid();

/**
 * \copybrief firstSentenceAsBriefFunction
 */
void copyBriefFromFirstSentenceAsBrief();

/**
 * \copybrief invalidReference
 */
void failCopyBriefFromInvalidReference();

/**
 * \copybrief copyBriefFromExplicitBrief
 */
void copyBriefFromCopyBrief();

// Copy details
/**
 * \brief Custom brief
 * \copydetails noDocFunction
 */
void copyDetailsFromNoDoc();

/**
 * \brief Custom brief
 * \copydetails docNoBriefFunction
 */
void copyDetailsFromDocNoBrief();

/**
 * \copydetails explicitBriefFunction
 */
void copyDetailsFromExplicitBrief();

/**
 * \copydetails firstSentenceAsBriefFunction
 */
void copyDetailsFromFirstSentenceAsBrief();

/**
 * \copydetails invalidReference
 */
void failCopyDetailsFromInvalidReference();

/**
 * \brief Details will be copied
 * \copydetails copyDetailsFromExplicitBrief
 */
void copyDetailsFromCopyBrief();

// Copy docs
/**
 * \copydoc noDocFunction
 */
void failCopyDocFromNoDoc();

/**
 * \copydoc docNoBriefFunction
 */
void failCopyDocFromDocNoBrief();

/**
 * \copydoc explicitBriefFunction
 */
void copyDocFromExplicitBrief();

/**
 * \copydoc firstSentenceAsBriefFunction
 */
void copyDocFromFirstSentenceAsBrief();

/**
 * \copydoc invalidReference
 */
void failCopyDocFromInvalidReference();

/**
 * \copydoc copyDocFromExplicitBrief
 */
void copyDocFromCopyBrief();

// Invalid reference for copy brief/details/docs
/**
 * \copybrief invalidReference
 * \copydetails invalidReference
 * \copydoc invalidReference
 */
void failInvalidReferenceCopyFunctions();

// Recursive reference for copy brief/details/docs

/**
 * @brief Final recursive brief
 */
void recursiveSourceFunctionA();

/**
 * \copybrief recursiveSourceFunctionA
 */
void recursiveSourceFunctionB();

/**
 * \copybrief recursiveSourceFunctionB
 */
void recursiveReferenceCopyFunction();

/**
 * \copybrief failCircularReferenceCopyFunction
 */
void failCircularSourceFunctionA();

/**
 * \copybrief failCircularSourceFunctionA
 */
void failCircularSourceFunctionB();

/**
 * \copybrief failCircularSourceFunctionB
 */
void failCircularReferenceCopyFunction();
