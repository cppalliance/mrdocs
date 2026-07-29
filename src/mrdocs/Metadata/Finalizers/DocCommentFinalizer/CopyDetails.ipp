// Impl fragment of DocCommentFinalizer.cpp (one TU): detail-copying logic. Included within namespace mrdocs.

void
DocCommentFinalizer::
copyDetails(Symbol& I)
{
    MRDOCS_CHECK_OR(!finalized_metadata_.contains(&I));
    finalized_metadata_.emplace(&I);

    report::trace(
            "Finalizing metadata for '{}'",
            corpus_.Corpus::qualifiedName(I));

    MRDOCS_CHECK_OR(I.doc);
    MRDOCS_CHECK_OR(!I.doc->Document.empty());
    DocComment& destDoc = *I.doc;

    llvm::SmallVector<doc::CopyDetailsInline, 16> copiedRefs;
    for (auto& block: destDoc.Document)
    {
        MRDOCS_CHECK_OR_CONTINUE(block->isParagraph());
        auto& para = dynamic_cast<doc::ParagraphBlock&>(*block);
        MRDOCS_CHECK_OR_CONTINUE(!para.children.empty());

        for (auto& text: para.children)
        {
            MRDOCS_CHECK_OR_CONTINUE(text->isCopyDetails());
            copiedRefs.emplace_back(dynamic_cast<doc::CopyDetailsInline&>(*text));
        }
        MRDOCS_CHECK_OR_CONTINUE(!copiedRefs.empty());
    }

    for (doc::CopyDetailsInline const& copied: copiedRefs)
    {
        // Find element
        auto resRef = corpus_.lookup(I.id, copied.string);
        if (!resRef)
        {
            if (config_.warnings &&
                config_.warnBrokenRef &&
                !refWarned_.contains({copied.string, I.Name}))
            {
                this->warn(
                    I,
                    "{}: Failed to copy metadata from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(I),
                    copied.string,
                    resRef.error().reason());
            }
            continue;
        }

        // Function to copy the metadata from a ranges
        // of source functions. This range might
        // contain more than one function if the
        // destination is an overload set.
        // We can't copy directly from the overload set
        // because its metadata is not created at this
        // step yet.
        auto copyInfoRangeMetadata = [&](llvm::ArrayRef<Symbol const*> srcInfoPtrs)
        {
            auto srcInfos = srcInfoPtrs
                            | std::views::transform(
                                [](Symbol const* ptr) -> Symbol const& {
                return *ptr;
            });

            // Ensure the source metadata is finalized
            for (auto& srcInfo: srcInfos)
            {
                auto& mutSrcInfo = const_cast<Symbol&>(srcInfo);
                copyDetails(mutSrcInfo);
            }

            // Copy returns only if destination is empty
            if (destDoc.returns.empty())
            {
                for (auto const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::ReturnsBlock const& returnsEl: srcInfo.doc->returns)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.returns, returnsEl));
                        destDoc.returns.push_back(returnsEl);
                    }
                }
            }

            // Copy only params that don't exist at the destination
            // documentation but that do exist in the destination
            // function parameters declaration.
            if (I.isFunction())
            {
                auto& destF = I.asFunction();
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.isFunction());
                    auto const& srcFunction = srcInfo.asFunction();
                    MRDOCS_CHECK_OR_CONTINUE(srcFunction.doc);
                    for (DocComment const& srcDocComment = *srcFunction.doc;
                         auto const& srcDocParam: srcDocComment.params)
                    {
                        // check if param doc doesn't already exist
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::none_of(
                                destDoc.params,
                                [&srcDocParam](doc::ParamBlock const& destDocParam) {
                            return srcDocParam.name == destDocParam.name;
                        }));

                        // check if param name exists in the destination function
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::any_of(
                                destF.Params,
                                [&srcDocParam](Param const& destParam) {
                            return srcDocParam.name == *destParam.Name;
                        }));

                        // Push the new param ot the
                        destDoc.params.push_back(srcDocParam);
                    }
                }
            }

            // Copy only tparams that don't exist at the destination
            // documentation but that do exist in the destination
            // template parameters.
            auto getTemplateInfo = [](Symbol& I) -> TemplateInfo const*
            {
                return visit(I, [](auto& I) -> TemplateInfo const* {
                    if constexpr (requires { I.Template; })
                    {
                        if (I.Template)
                        {
                            return &*I.Template;
                        }
                    }
                    return nullptr;
                });
            };


            if (auto const destTemplateInfo = getTemplateInfo(I))
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (DocComment const& srcDocComment = *srcInfo.doc;
                         auto const& srcTParam: srcDocComment.tparams)
                    {
                        // tparam doesn't already exist at the destination
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::none_of(
                                destDoc.tparams,
                                [&srcTParam](doc::TParamBlock const& destTParam) {
                            return srcTParam.name == destTParam.name;
                        }));

                        // TParam name exists in the destination definition
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::any_of(
                                destTemplateInfo->Params,
                                [&srcTParam](
                                    Polymorphic<TParam> const& destTParam) {
                            return srcTParam.name == destTParam->Name;
                        }));

                        // Push the new param
                        destDoc.tparams.push_back(srcTParam);
                    }
                }
            }

            // Copy exceptions only if destination exceptions are empty
            // and the destination is not noexcept
            bool const destIsNoExcept =
                I.isFunction() &&
                I.asFunction().Noexcept.Kind == NoexceptKind::False;
            if (destDoc.exceptions.empty() &&
                !destIsNoExcept)
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::ThrowsBlock const& exceptionsEl: srcInfo.doc->exceptions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.exceptions, exceptionsEl));
                        destDoc.exceptions.push_back(exceptionsEl);
                    }
                }
            }

            // Copy sees only if destination sees are empty
            if (destDoc.sees.empty())
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::SeeBlock const& seesEl: srcInfo.doc->sees)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.sees, seesEl));
                        destDoc.sees.push_back(seesEl);
                    }
                }
            }

            // Copy preconditions only if destination preconditions is empty
            if (destDoc.preconditions.empty())
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::PreconditionBlock const& preconditionsEl: srcInfo.doc->preconditions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.preconditions, preconditionsEl));
                        destDoc.preconditions.push_back(preconditionsEl);
                    }
                }
            }

            // Copy postconditions only if destination postconditions is empty
            if (destDoc.postconditions.empty())
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::PostconditionBlock const& postconditionsEl:
                         srcInfo.doc->postconditions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(
                            destDoc.postconditions,
                            postconditionsEl));
                        destDoc.postconditions.push_back(postconditionsEl);
                    }
                }
            }
        };

        // Ensure the source metadata is finalized
        Symbol const& res = *resRef;
        if (!res.isOverloads())
        {
            // If it's a single element, we check the element doc
            if (!res.doc)
            {
                if (config_.warnings &&
                    config_.warnBrokenRef &&
                    !refWarned_.contains({copied.string, I.Name}))
                {
                    auto resPrimaryLoc = getPrimaryLocation(res);
                    this->warn(
                        I,
                        "{}: Failed to copy metadata from {} '{}' (no documentation available).\n"
                        "    No metadata available.\n"
                        "        {}:{}\n"
                        "        Note: No documentation available for '{}'.",
                        corpus_.Corpus::qualifiedName(I),
                        toString(res.Kind),
                        copied.string,
                        resPrimaryLoc->FullPath,
                        resPrimaryLoc->LineNumber,
                        corpus_.Corpus::qualifiedName(res));
                }
                continue;
            }
            llvm::SmallVector<Symbol const*, 1> srcInfos = { &res };
            copyInfoRangeMetadata(srcInfos);
        }
        else
        {
            auto& OI = res.asOverloads();
            llvm::SmallVector<Symbol const*, 16> srcInfos;
            srcInfos.reserve(OI.Members.size());
            for (auto& memberID: OI.Members)
            {
                Symbol* member = corpus_.find(memberID);
                MRDOCS_CHECK_OR_CONTINUE(member);
                srcInfos.push_back(member);
            }
            copyInfoRangeMetadata(srcInfos);
        }
    }

    if (I.doc)
    {
        copyDetails(I,*I.doc);
    }
}

void
DocCommentFinalizer::
copyDetails(Symbol const& ctx, DocComment& doc)
{
    for (auto blockIt = doc.Document.begin(); blockIt != doc.Document.end();)
    {
        // Get paragraph
        auto& block = *blockIt;
        if (!block->isParagraph())
        {
            ++blockIt;
            continue;
        }
        auto& para = block->asParagraph();
        if (para.empty())
        {
            ++blockIt;
            continue;
        }

        // Find copydetails command
        Optional<doc::CopyDetailsInline> copied;
        for (auto inlineIt = para.children.begin(); inlineIt != para.children.end();)
        {
            // Find copydoc command
            auto& inlineEl = *inlineIt;
            if (!inlineEl->isCopyDetails())
            {
                ++inlineIt;
                continue;
            }
            // Copy reference
            copied = inlineEl->asCopyDetails();

            // Remove copied node from the inlineEl
            /* it2 = */ para.children.erase(inlineIt);
            break;
        }

        // Trim the paragraph after removing the copydetails command
        doc::trim(para.asInlineContainer());

        // Remove empty children from the paragraph
        std::erase_if(para.children, [](Polymorphic<doc::Inline> const& child) {
            return doc::isEmpty(child);
        });

        // We should merge consecutive text nodes that have exactly the
        // same terminal kind

        // Remove the entire paragraph block from the doc if it is empty
        if (para.empty())
        {
            blockIt = doc.Document.erase(blockIt);
            MRDOCS_CHECK_OR_CONTINUE(copied);
        }

        // Nothing to copy: continue to the next block
        if (!copied)
        {
            ++blockIt;
            continue;
        }

        // Find the node to copy from
        auto resRef = corpus_.lookup(ctx.id, copied->string);
        if (!resRef)
        {
            if (config_.warnings &&
                config_.warnBrokenRef &&
                !refWarned_.contains({copied->string, ctx.Name}))
            {
                this->warn(
                    ctx,
                    "{}: Failed to copy documentation from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(ctx),
                    copied->string,
                    resRef.error().reason());
            }
            continue;
        }

        // Ensure the source node is finalized
        Symbol const& res = *resRef;
        resolveReferences(const_cast<Symbol&>(res));

        // Check if there's any documentation details to copy
        if (!res.doc)
        {
            if (config_.warnings &&
                config_.warnBrokenRef &&
                !refWarned_.contains({copied->string, ctx.Name}))
            {
                auto resPrimaryLoc = getPrimaryLocation(res);
                this->warn(
                    ctx,
                    "{}: Failed to copy documentation from {} '{}' (no documentation available).\n"
                    "    No documentation available.\n"
                    "        {}:{}\n"
                    "        Note: No documentation available for '{}'.",
                    corpus_.Corpus::qualifiedName(ctx),
                    toString(res.Kind),
                    copied->string,
                    resPrimaryLoc->FullPath,
                    resPrimaryLoc->LineNumber,
                    corpus_.Corpus::qualifiedName(res));
            }
            continue;
        }

        // Copy detail blocks from source to destination to
        // the same position in the destination
        DocComment const& src = *res.doc;
        if (!src.Document.empty())
        {
            blockIt = doc.Document.insert(blockIt, src.Document.begin(), src.Document.end());
            blockIt += src.Document.size();
        }
    }
}
