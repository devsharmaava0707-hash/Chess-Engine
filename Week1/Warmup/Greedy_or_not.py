# Write your code here
def scorecalculator(st,end,nums):
    if st==end:
        return nums[st]
    if st>end :
        return 0;
    int_i=nums[st]+min(scorecalculator(st+1,end-1,nums),scorecalculator(st+2,end,nums))
    int_j=nums[end]+min(scorecalculator(st+1,end-1,nums),scorecalculator(st,end-2,nums))
    return max(int_i,int_j)

n = int(input())

nums = []
while len(nums) < n:
    nums.extend(map(int, input().split()))
score_1=scorecalculator(0,n-1,nums)
score_2=sum(nums)-score_1
if score_1 > score_2:
    print("Player 1 wins")
elif score_2 > score_1:
    print("Player 2 wins")
else:
    print("Its a draw")
    
