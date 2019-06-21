#include "stub_process_factory.h"

namespace mp = multipass;
namespace mpt = multipass::test;

std::unique_ptr<mp::Process> mpt::StubProcessFactory::create_process(std::unique_ptr<ProcessSpec>&&) const
{
    return std::make_unique<mpt::StubProcess>();
}

mpt::StubProcessFactory& mpt::StubProcessFactory::stub_instance()
{
    return dynamic_cast<mpt::StubProcessFactory&>(instance());
}
