// Based on AST matching sample by Eli Bendersky (eliben@gmail.com)
#include <string>
#include <iostream>
#include <fstream>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory MatcherSampleCategory("Matcher Sample");

class BindGen {
public:

    BindGen(const std::string java_file, const std::string c_file) {
        // FIXME: Handle errors
        ofjava.open(java_file);
        ofc.open(c_file);

        ofjava << "public class LapackJNI {" << std::endl;
        ofjava << "\tstatic {" << std::endl;
        ofjava << "\t\tSystem.loadLibrary(\"LapackJNI\");" << std::endl;
        ofjava << "\t}" << std::endl << std::endl;
    }

    void addDecl(const FunctionDecl *Decl) {
        std::string retType = asJavaType(Decl->getReturnType());
        std::string funName = Decl->getNameInfo().getName().getAsString();
        ofjava << "\t public native " << retType << " " << funName << "(\n";
        unsigned numParams = Decl->getNumParams();
        if (numParams > 0) {
            for (unsigned i = 0; i < Decl->getNumParams() - 1; ++i) {
                ofjava << toJavaParam(Decl->getParamDecl(i)) << ", ";
            }
            ofjava << toJavaParam(Decl->getParamDecl(numParams - 1));
        }
    }

    void finish() {
        ofjava << "}\n";
        ofjava.close();
    }

private:
    std::ofstream ofjava;
    std::ofstream ofc;

    std::string toJavaParam(const ParmVarDecl *PVD) {
        std::string t = asJavaType(PVD->getOriginalType());
        return t + " " + PVD->getNameAsString();
    }

    std::string asJavaType(QualType QT) {
        const Type *t = QT.getTypePtr();
        if (t->isPointerType()) {
            return "long";
        } else if (t->isEnumeralType()) {
            return "int";
        } else if (t->isBuiltinType()){
            LangOptions lo = LangOptions();
            PrintingPolicy pp = PrintingPolicy(lo);
            return t->getAsPlaceholderType()->getName(pp);
        } else {
            // FIXME
            return "???";
        }
    }
};

class DeclHandler : public MatchFinder::MatchCallback {
public:
    DeclHandler(BindGen *BG_) {
        BG = BG_;
    }

    virtual void run(const MatchFinder::MatchResult &Result) {
        const FunctionDecl *Decl = Result.Nodes.getNodeAs<FunctionDecl>("funDecl");
        if ( ! (Decl->isThisDeclarationADefinition() && Decl->hasPrototype())) {
            std::cout << "Found " << Decl->getNameInfo().getName().getAsString() << std::endl;
            std::cout << "  " << Decl->getReturnType().getAsString();
            for (unsigned i = 0; i < Decl->getNumParams(); ++i) {
                std::cout << " " << Decl->getParamDecl(i)->getOriginalType().getAsString();
            }
            std::cout << std::endl;
            BG->addDecl(Decl);
        }
    }

private:
    BindGen *BG;
};

// Implements an ASTConsumer for reading the AST produced by the Clang parser
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(BindGen &BG) : HandlerForFunDecl(&BG) {
    // The MatchFinder will look for function declaration
    Matcher.addMatcher(functionDecl().bind("funDecl"), &HandlerForFunDecl);
  }

  // This is called for every translation unit. The MatchFinder is then run on the AST produced by the translation unit.
  void HandleTranslationUnit(ASTContext &Context) override {
    Matcher.matchAST(Context);
  }

private:
  DeclHandler HandlerForFunDecl;
  MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:

  MyFrontendAction() : BG("LapackJNI.java", "LapackJNI.c") {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    return llvm::make_unique<MyASTConsumer>(BG);
  }

private:
  BindGen BG;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, MatcherSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
