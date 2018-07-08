#include "statements.h"
#include "template_impl.h"
#include "value_visitors.h"


namespace jinja2
{

void ForStatement::Render(OutStream& os, RenderContext& values)
{
    InternalValue loopVal = m_value->Evaluate(values);

    auto& context = values.EnterScope();

    InternalValueMap loopVar;
    context["loop"] = MapAdapter::CreateAdapter(&loopVar);

    bool isConverted = false;
    auto loopItems = ConvertToList(loopVal, InternalValue(), isConverted);
    if (!isConverted)
        return;

    if (m_ifExpr)
    {
        auto& tempContext = values.EnterScope();
        InternalValueList newLoopItems;
        for (auto& curValue : loopItems)
        {
            if (m_vars.size() > 1)
            {
                for (auto& varName : m_vars)
                    tempContext[varName] = Subscript(curValue, varName);
            }
            else
                tempContext[m_vars[0]] = curValue;

            if (ConvertToBool(m_ifExpr->Evaluate(values)))
                newLoopItems.push_back(curValue);
        }
        values.ExitScope();

        loopItems = ListAdapter::CreateAdapter(std::move(newLoopItems));
    }

    int64_t itemsNum = static_cast<int64_t>(loopItems.GetSize());
    loopVar["length"] = InternalValue(itemsNum);
    bool loopRendered = false;
    for (int64_t itemIdx = 0; itemIdx != itemsNum; ++ itemIdx)
    {
        loopRendered = true;
        loopVar["index"] = InternalValue(itemIdx + 1);
        loopVar["index0"] = InternalValue(itemIdx);
        loopVar["first"] = InternalValue(itemIdx == 0);
        loopVar["last"] = InternalValue(itemIdx == itemsNum - 1);
        if (itemIdx != 0)
            loopVar["previtem"] = loopItems.GetValueByIndex(static_cast<size_t>(itemIdx - 1));
        if (itemIdx != itemsNum - 1)
            loopVar["nextitem"] = loopItems.GetValueByIndex(static_cast<size_t>(itemIdx + 1));
        else
            loopVar.erase("nextitem");

        const auto& curValue = loopItems.GetValueByIndex(static_cast<size_t>(itemIdx));
        if (m_vars.size() > 1)
        {
            for (auto& varName : m_vars)
                context[varName] = Subscript(curValue, varName);
        }
        else
            context[m_vars[0]] = curValue;

        m_mainBody->Render(os, values);
    }

    if (!loopRendered && m_elseBody)
        m_elseBody->Render(os, values);

    values.ExitScope();
}

void IfStatement::Render(OutStream& os, RenderContext& values)
{
    InternalValue val = m_expr->Evaluate(values);
    bool isTrue = Apply<visitors::BooleanEvaluator>(val);

    if (isTrue)
    {
        m_mainBody->Render(os, values);
        return;
    }

    for (auto& b : m_elseBranches)
    {
        if (b->ShouldRender(values))
        {
            b->Render(os, values);
            break;
        }
    }
}

bool ElseBranchStatement::ShouldRender(RenderContext& values) const
{
    if (!m_expr)
        return true;

    return Apply<visitors::BooleanEvaluator>(m_expr->Evaluate(values));
}

void ElseBranchStatement::Render(OutStream& os, RenderContext& values)
{
    m_mainBody->Render(os, values);
}

void SetStatement::Render(OutStream&, RenderContext& values)
{
   if (m_expr)
   {
       InternalValue val = m_expr->Evaluate(values);
       if (m_fields.size() == 1)
           values.GetCurrentScope()[m_fields[0]] = val;
       else
       {
           for (auto& name : m_fields)
               values.GetCurrentScope()[name] = Subscript(val, name);
       }
   }
}

class BlocksRenderer : public RendererBase
{
public:
    virtual void RenderBlock(const std::string& blockName, OutStream& os, RenderContext& values) = 0;
};

void ParentBlockStatement::Render(OutStream& os, RenderContext& values)
{
    RenderContext innerContext = values.Clone(m_isScoped);
    bool found = false;
    auto parentTplVal = values.FindValue("$$__parent_template", found);
    if (!found)
        return;

    auto parentTplPtr = boost::get<RendererBase*>(&parentTplVal->second);
    if (parentTplPtr == nullptr)
        return;

    BlocksRenderer* blockRenderer = static_cast<BlocksRenderer*>(*parentTplPtr);

    auto& scope = innerContext.EnterScope();
    scope["$$__super_block"] = static_cast<RendererBase*>(this);
    blockRenderer->RenderBlock(m_name, os, innerContext);
    innerContext.ExitScope();
}

void BlockStatement::Render(OutStream& os, RenderContext& values)
{
    m_mainBody->Render(os, values);
}

template<typename CharT>
class ParentTemplateRenderer : public BlocksRenderer
{
public:
    ParentTemplateRenderer(std::shared_ptr<TemplateImpl<CharT>> tpl, ExtendsStatement::BlocksCollection* blocks)
        : m_template(tpl)
        , m_blocks(blocks)
    {
    }

    void Render(OutStream& os, RenderContext& values) override
    {
        auto& scope = values.GetCurrentScope();
        scope["$$__parent_template"] = static_cast<RendererBase*>(this);
        m_template->GetRenderer()->Render(os, values);
    }

    void RenderBlock(const std::string& blockName, OutStream& os, RenderContext& values) override
    {
        auto p = m_blocks->find(blockName);
        if (p == m_blocks->end())
            return;

        p->second->Render(os, values);
    }

private:
    std::shared_ptr<TemplateImpl<CharT>> m_template;
    ExtendsStatement::BlocksCollection* m_blocks;
};

struct TemplateImplVisitor : public boost::static_visitor<RendererPtr>
{
    ExtendsStatement::BlocksCollection* m_blocks;

    TemplateImplVisitor(ExtendsStatement::BlocksCollection* blocks)
        : m_blocks(blocks)
    {}

    template<typename CharT>
    RendererPtr operator()(std::shared_ptr<TemplateImpl<CharT>> tpl) const
    {
        return std::make_shared<ParentTemplateRenderer<CharT>>(tpl, m_blocks);
    }

    RendererPtr operator()(EmptyValue) const
    {
        return RendererPtr();
    }
};

void ExtendsStatement::Render(OutStream& os, RenderContext& values)
{
    if (!m_isPath)
    {
        // FIXME: Implement processing of templates
        return;
    }
    auto tpl = values.GetRendererCallback()->LoadTemplate(m_templateName);
    auto renderer = boost::apply_visitor(TemplateImplVisitor(&m_blocks), tpl);
    if (renderer)
        renderer->Render(os, values);
}

} // jinja2
