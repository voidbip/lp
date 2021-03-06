#include <iostream>
#include <stdio.h>
#include <stack>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "lapacke.h"
#include "cblas.h"
#include "matrix.h"
#include <unordered_map>
#include <assert.h>

using namespace std;

template <class T, class Q>
class BB {
    public:
        Q C;
        T container;
        int branch(T ps){
            unsigned int pop;
            this->C.push(ps);
            pop = this->C.top();
            this->C.pop();
            printf("Got: %d\r\n",pop);
            return(0);
        }
};


class PartialSolution {
    public:
        virtual int eval() = 0;
        virtual int branch() = 0;
};

class ILP : public PartialSolution{
    public:
        int eval(){ return 0;}
        int branch(){ return 0;}
        void print_matrix(const double *A,unsigned int M, unsigned int N);

        /** \brief Minimize c'x st. Ax=b. Using Simplex Algorithm.
         * \fn Vector& solve_simplex(Matrix &A, Vector &b, Vector &c);
         * \param[in] A constraint matrix.
         * \param[in] b constraint vector.
         * \param[in] c cost vector.
         * \return Minimizing vector.
         *
         */

        Vector<double>& solve_simplex(Matrix<double> &A, Vector<double> &b, Vector<double> &c);
        int solve_simplex(const double *A, const double *b, double *c, unsigned int M, unsigned int N);
    private:
        int simplex_core(const double *A, const double *b, double *c, unsigned int *bv, int M, int N);
        int price(double *r, unsigned int *dv, const double *A, const double *c, const unsigned int *bv, unsigned int M,unsigned int N);
        int findBasisToEnter(const double *r,unsigned int N);
        int findBasisToLeave(const double *A,const double *b, const unsigned int j, unsigned int M);
        int getBasicSolution(const double *A,const unsigned int *bv, double *b,unsigned int M);
        int copybycols(double *dest, const double *src,const unsigned int *col, unsigned int M, unsigned int numcols);

};


int ILP::getBasicSolution(const double *A,const unsigned int *bv,double *b,unsigned int M){
    double *B;
    int *pivot;

    B = (double *)malloc(M*M*sizeof(double)); // basis
    pivot = (int *)malloc(M*sizeof(int));


    copybycols(B,A,bv,M,M);
    // compute LU factorization of B.
    LAPACKE_dgetrf(LAPACK_COL_MAJOR,M,M,B,M,pivot);

    // solve this system. cbv is now lambda.
    LAPACKE_dgetrs(LAPACK_COL_MAJOR,'N',M,1,B,M,pivot,b,M);

    free(B);
    free(pivot);
    return(0);
}


int ILP::findBasisToEnter(const double *r,unsigned int N){
    unsigned int i;
    for(i=0;i<N;i++){
        if(r[i]<0)
            return(i);
    }
    return(-1);
}

int ILP::findBasisToLeave(const double *A,const double *b, const unsigned int j,unsigned int M){
    double ratio;
    double min_ratio;
    unsigned int i;
    int i_out = -1;
    min_ratio = INFINITY;
    for(i=0;i<M;i++){
        if((ratio=b[i]/A[(j*M)+i])>0){
            if(ratio<min_ratio){
                min_ratio=ratio;
                i_out = i;
            }
        }
    }
    return(i_out);
}

int ILP::price(double *r, unsigned int *dv, const double *A, const double *c, const unsigned int *bv, unsigned int M,unsigned int N){
    double *lambda, *B,*cbv;
    int *pivot;
    bool flag;
    unsigned int i,j,k;
    lambda = (double *)malloc(M*sizeof(double));
    pivot = (int *)malloc(M*sizeof(int));
    B = (double *)malloc(M*M*sizeof(double)); // basis
    cbv = (double *)malloc(M*sizeof(double));


    copybycols(B,A,bv,M,M);
    copybycols(cbv,c,bv,1,M);

    // compute LU factorization of B.
    LAPACKE_dgetrf(LAPACK_COL_MAJOR,M,M,B,M,pivot);

    // solve this system. cbv is now lambda.
    LAPACKE_dgetrs(LAPACK_COL_MAJOR,'T',M,1,B,M,pivot,cbv,M);
    for(i=0;i<M;i++){
        lambda[i] = cbv[i];
    }


    k=0;
    for(i=0;i<N;i++){
        flag = 0;

        // is i in the basis?
        for(j=0;j<M;j++){
            if(i==bv[j]){
                flag = true;
                break;
            }
        }

        // if not compute price. 
        if(!flag){
          r[k]=c[i]-cblas_ddot(M,lambda,1,&A[i*M],1);
          dv[k] = i;
          k++;
        }

        
        
    }


    //loop through non-basis vectors and compute price. 

   free(pivot);
   free(lambda);
   free(cbv);
   free(B);
   return(0);

}


void ILP::print_matrix(const double *A,unsigned int M, unsigned int N){
    unsigned int i,j;
    for(i=0;i<M;i++){
        for(j=0;j<N;j++){
            printf("%f\t",A[(j*M)+i]);
        }
        printf("\r\n");
    }
}

int ILP::copybycols(double *dest, const double *src,const unsigned int *col, unsigned int M, unsigned int numcols){
    unsigned int i;
    for(i=0;i<numcols;i++){
        memcpy(&dest[i*M],&src[M*col[i]],M*sizeof(double));
    }
    return(0);
}

int ILP::simplex_core(const double *A, const double *b, double *c, unsigned int *bv, int M, int N) {
    int i,j,k;
    double *r,*bcpy;
    unsigned int *dv;
    int dvidx;

    if(M>N){
        assert(M>N);
        printf("Number of rows must be larger than number of columns\r\n");
        return(1);
    }
    

    r = (double *)malloc((N-M)*sizeof(double));
    dv = (unsigned int *)malloc((N-M)*sizeof(unsigned int));
    bcpy = (double *)malloc(M*sizeof(double));

    memcpy(bcpy,b,M*sizeof(double));
    
    price(r, dv, A, c, bv, M, N);

    while(0<=(dvidx=findBasisToEnter(r,N-M))){
        j=dv[dvidx];
        if((k=findBasisToLeave(A,bcpy,j,M))<0){
            printf("Problem is unbounded from below.");
            exit(1);
        }

        dv[dvidx]=bv[k];
        bv[k]=j;

        // b = linsolve(A(:,bv),b)

        getBasicSolution(A,bv,bcpy,M);

        price(r, dv, A, c, bv, M, N);
        

    }

    
    memset(c,0,N*sizeof(double));
    for(i=0;i<M;i++){
        c[bv[i]]=bcpy[i];
    }

    free(r);
    free(dv);
    free(bcpy);

    return(0);
}

int ILP::solve_simplex(const double *A, const double *b, double *c, unsigned int M, unsigned int N){
    unsigned int i,k,*bv;
    double *aa,*ca;
    aa = (double *) calloc(M*(M+N),sizeof(double));
    ca = (double *) calloc((M+N),sizeof(double));
    bv = (unsigned int *) calloc(M,sizeof(unsigned int));
    memcpy(aa,A,M*N*sizeof(double));


    /* Lets now augment aa with the identity matrix, though these should be chased out of the basis eventually */
    for(i=0;i<M;i++){
        aa[(M*N)+(i*M)+i]=1;
    }

    k=0;
    for(i=N;i<N+M;i++){
        ca[i]=1;
        bv[k++]=i;
    }

    // Solve the phase I problem. to obtail and BFS
    simplex_core(aa,b,ca,bv,M,M+N);

    // Take that BFS and solve the original problem.
    simplex_core(A,b,c,bv,M,N);

    print_matrix(c,N,1);





    free(aa);
    free(ca);
    free(bv);
    return 0;
}


Vector<double>& ILP::solve_simplex(Matrix<double> &A, Vector<double> &b, Vector<double> &c){
    solve_simplex(A.getData(),b.getData(),c.getData(),A.getNumRows(),A.getNumCols());
    return b;
}


#define NAMECARD 0
#define ROWCARD 1
#define COLCARD 2
#define RHSCARD 3
#define BOUNDCARD 4


class Entity {
    public:
        int setName(char *n);
        int setNum(const unsigned int num);
        int setType(const char t);
        char getType();
        string& getName();
        unsigned int getNum();
    private:
        string name;
        unsigned int num;
        char type;
};



int Entity::setName(char *n){
    name=n;
    return(0);
}

int Entity::setType(const char t){
    type = t;
    return(0);
}

int Entity::setNum(const unsigned int n){
    num=n;
    return(0);
}

string& Entity::getName(){
    return name;
}

char Entity::getType(){
    return type;
}

unsigned int Entity::getNum(){
    return num;
}

class Constraint : public Entity {
    public:
        Constraint(char *n, const char t,const unsigned int num);
    private:
};

Constraint::Constraint(char *n, const char t,const unsigned int num){
    setName(n);
    setType(t);
    setNum(num);
}

class Variable : public Entity {
    public:
        Variable(unsigned int n){ setNum(n); };
    private:
};

unsigned int stris(const char *s1,const char *s2){
    return(strncmp(s1,s2,strlen(s2))==0);
}

unsigned int readMPS(FILE *&fp,double *&A,double *&b,double *&c, unordered_map<string,Entity * > &ConstraintSet , unordered_map<string, Entity * > &VariableSet , unordered_map<string, Entity * > &RHSSet ){
    char str[1024];
    char chardata[1024],name[1024],type;
    char varname[1024], constraint[2][1024];
    double f[2];
    unsigned int card,numcols,lineno=0,i;
    unsigned int numVars = 0, numConstraints = 0;
    string cost;
    numVars = 0;
    numConstraints = 0;

    while(!feof(fp)){
        fgets(str,1024,fp);
        lineno++;
        if(stris(str,"ENDATA")){
            break;
        }
        else if(str[0]=='*'){
            continue;
        }
        else if(str[0]!=' '){
            if(stris(str,"NAME")){
                card=NAMECARD;
                sscanf(str,"%s %s",chardata,name);
                printf("GOT: %s , %s\r\n",chardata,name);
            }
            else if(stris(str,"ROWS")){
                card=ROWCARD;
            }
            else if(stris(str,"COLUMNS")){
                card=COLCARD;
            }
            else if(stris(str,"RHS")){
                numVars=0;
                card=RHSCARD;
            }
            else if(stris(str,"BOUNDS")){
                card=BOUNDCARD;
            }
        }
        else if(str[0]==' '){
            switch(card){
                case NAMECARD:
                    printf("  manage name card\r\n");
                break;
                case ROWCARD:
                    if(sscanf(str," %c %s",&type,chardata) != 2){
                        printf("Parse eror in ROWCARD");
                        exit(1);
                    }
                    if(type!='N')
                        ConstraintSet[chardata] = new Constraint(chardata,type,numConstraints++);
                    else
                        cost = chardata;
                break;
                case COLCARD:
                    numcols=sscanf(str," %s %s %lf %s %lf",varname, constraint[0], &f[0], constraint[1], &f[1]);
                    /* Need to add logic to dynamically enlarge the A matrix as necessary. */
                    if((numcols==3)||(numcols==5)){
                        if(VariableSet.end() == VariableSet.find(varname)){
                            VariableSet[varname] = new Variable(numVars);
                            numVars++;
                            A = (double *)realloc(A,numVars*numConstraints*sizeof(double));
                            c = (double *)realloc(c,numVars*sizeof(double));
                        }
                        for(i=0;i<=(numcols==5);i++){
                            if(ConstraintSet.end() != ConstraintSet.find(constraint[i])){
                                switch(ConstraintSet[constraint[i]]->getType()){
                                    case 'L':
                                        A[((VariableSet[varname]->getNum())*numConstraints)+ConstraintSet[constraint[i]]->getNum()] = f[i];
                                    break;
                                    case 'E':
                                        A[((VariableSet[varname]->getNum())*numConstraints)+ConstraintSet[constraint[i]]->getNum()] = f[i];
                                    break;
                                    case 'G':
                                        A[((VariableSet[varname]->getNum())*numConstraints)+ConstraintSet[constraint[i]]->getNum()] = -f[i];
                                    break;
                                    default:
                                        printf("Unknown constraint type online %d",lineno);
                                        return(1);
                                    break;
                                }
                            }
                            else if(cost==constraint[i]){
                                c[VariableSet[varname]->getNum()] = f[i];
                            }
                        }
                    }
                    else {
                        printf("Parse error in Column Card on line: %d",lineno);
                        exit(1);
                    }

                break;
                case RHSCARD:
                    numcols=sscanf(str," %s %s %lf %s %lf",varname, constraint[0], &f[0], constraint[1], &f[1]);

                    /* Need to add logic to dynamically enlarge the A matrix as necessary. */
                    if((numcols==3)||(numcols==5)){
                        if(RHSSet.end() == RHSSet.find(varname)){
                            RHSSet[varname] = new Variable(numVars);
                            numVars++;
                            b = (double *)realloc(b,numVars*numConstraints*sizeof(double));
                        }
                        for(i=0;i<=(numcols==5);i++){
                            if(ConstraintSet.end() != ConstraintSet.find(constraint[i])){
                                b[((RHSSet[varname]->getNum())*numConstraints)+ConstraintSet[constraint[i]]->getNum()] = (ConstraintSet[constraint[i]]->getType() == 'G') ? -f[i] : f[i];
                            }
                            else{
                               printf("Error Constraint not found in evaluating RHS card."); 
                               return(1);
                            }
                        }
                    }
                    else {
                        printf("Parse error in Column Card on line: %d",lineno);
                        exit(1);
                    }

                break;
                case BOUNDCARD:
                    printf("  manage bound card\r\n");
                break;
                default:
                    printf("  unknown card on line %d\r\n",lineno);
                break;
            }
        }
        else{
            printf("Parse error on line %d\r\n",lineno);
            return(0);
        }
        // printf("%s",str);
    }

    return(0);
}

int main(int argc, char **argv)
{
    FILE *fp;
    ILP *p = new ILP;
    //double a[9] = {2,1,2,3,3,1};
    //double b[4] = {4,3};
    //double c[3] = {4,1,1};
    double *A,*b,*c;
    A = b = c = NULL;


    unordered_map<string,Entity * > ConstraintSet;
    unordered_map<string,Entity * > VariableSet;
    unordered_map<string,Entity * > RHSSet;

    if(argc < 2){
        printf("usage: simplex <filename.mps> \r\n");
        return(1);
    }


    if(NULL == (fp = fopen(argv[1],"r"))){
        printf("failed to open file: %s \r\n",argv[1]);
        return(1);
    }
    readMPS(fp,A,b,c,ConstraintSet,VariableSet,RHSSet);

   printf("Printing matrix of %lu rows and %lu cols...\r\n",ConstraintSet.size(),VariableSet.size()); 
   p->print_matrix(A,ConstraintSet.size(),VariableSet.size());
   p->print_matrix(c,VariableSet.size(),1);

   p->print_matrix(b,ConstraintSet.size(),1);
    delete p;

    fclose(fp);

    /* Matrix<double> A(a,2,3);
    Vector<double> B(b,2);
    Vector<double> C(c,3);

    p->solve_simplex(A,B,C); */

}

